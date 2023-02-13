#include "l1_data_cache.h"

struct DEBUG_L2_memRsp :public pipe_reg_base, public cache_building_block{
    DEBUG_L2_memRsp(){}
    DEBUG_L2_memRsp(L2_2_dcache_memRsp inf, enum TL_UH_D_opcode opcode)
        :m_inf(inf),m_opcode(opcode){}

    L2_2_dcache_memRsp m_inf;
    enum TL_UH_D_opcode m_opcode;
};
class DEBUG_L2_model : public cache_building_block{
public:

    bool return_Q_is_empty(){
        return m_return_Q.empty();
    }

    L2_2_dcache_memRsp DEBUG_serial_pop(){
        assert(!m_return_Q.empty());
        auto value = m_return_Q.front();
        m_return_Q.pop_front();
        return value;
    }

    //非随机，一直提供次次尾部的元素
    L2_2_dcache_memRsp DEBUG_random_pop(){
        std::deque<L2_2_dcache_memRsp>::iterator value_iter;
        if (m_return_Q.size()<=2){
            value_iter = m_return_Q.begin();
        }else{
            value_iter = m_return_Q.begin() + 2;
        }
        auto value = *value_iter;
        m_return_Q.erase(value_iter);
        return value;
    }

    void DEBUG_L2_memReq_process(dcache_2_L2_memReq req, cycle_t time){
        if (!m_process_Q[m_minimal_process_latency-1].is_valid()){
            enum TL_UH_D_opcode return_op;
            if(req.a_opcode == Get || req.a_opcode == ArithmeticData 
                || req.a_opcode == LogicalData){
                return_op = AccessAckData;
            }else{
                return_op = AccessAck;
            }
            L2_2_dcache_memRsp new_memRsp = L2_2_dcache_memRsp(req.a_source);
            DEBUG_L2_memRsp new_L2_return = DEBUG_L2_memRsp(new_memRsp,return_op);
            m_process_Q[m_minimal_process_latency-1] = new_L2_return;

            //debug info
            std::cout << "memReq out at " << time;
            std::cout << ", TLopcode=" << req.a_opcode <<std::endl;
        }
    }

    void cycle(){
        if (m_process_Q[0].is_valid()){
            if (m_process_Q[0].m_opcode == AccessAckData){
                m_return_Q.push_back(m_process_Q[0].m_inf);
            }
        }
        for (unsigned stage = 0; stage < m_minimal_process_latency - 1; ++stage){
            m_process_Q[stage] = m_process_Q[stage + 1];
            m_process_Q[stage + 1].invalidate();
        }
    }
private:
    //从L2 memReq到L2 memRsp的最小间隔周期
    constexpr static int m_minimal_process_latency = 3;
    std::array<DEBUG_L2_memRsp,m_minimal_process_latency> m_process_Q {};
    std::deque<L2_2_dcache_memRsp> m_return_Q;
};

class test_env {

public:
    test_env(){
        DEBUG_init_stimuli();
    }

    void DEBUG_print_coreRsp_pop(cycle_t time){
        if (dcache.m_coreRsp_Q.m_Q.size() != 0){
            dcache.m_coreRsp_Q.DEBUG_print();
            dcache.m_coreRsp_Q.m_Q.pop_front();
        }
        //dcache.m_coreRsp_ready = true;
    }

    void DEBUG_cycle(cycle_t time){
        DEBUG_print_coreRsp_pop(time);
        L2.cycle();
        if(!dcache.m_memReq_Q.is_empty()){
            L2.DEBUG_L2_memReq_process(dcache.m_memReq_Q.m_Q.front(), time);
            dcache.m_memReq_Q.m_Q.pop_front();
        }
        dcache.cycle(time);
        if(!L2.return_Q_is_empty()){
            dcache.m_memRsp_Q.m_Q.push_back(L2.DEBUG_serial_pop());
        }
        if(!dcache.m_coreReq.is_valid() && !coreReq_stimuli.empty()){
            auto size = coreReq_stimuli.size();
            dcache.m_coreReq.update_with(coreReq_stimuli.front());
            coreReq_stimuli.pop_front();
        }
    }

    //TODO 丰富这个函数的个数，创造更多测试
    void DEBUG_init_stimuli(){
        std::array<u_int32_t,32> p_addr = {};
        std::array<bool,32> p_mask = {true};
        for(int i=0;i<10;++i){
            LSU_2_dcache_coreReq coreReq=LSU_2_dcache_coreReq(Read,0,random(0,31),i,0xff02,p_addr,p_mask);
            coreReq_stimuli.push_back(coreReq);
        }
    }

    l1_data_cache dcache;
private:
    std::deque<LSU_2_dcache_coreReq> coreReq_stimuli;
    DEBUG_L2_model L2;
};

int main() {
    std::cout << "modeling cache now" << std::endl;
    test_env tb;
    tb.dcache.m_tag_array.DEBUG_random_initialize(100);
    for (int i = 100 ; i < 120 ; ++i){
        tb.DEBUG_cycle(i);
    }
    
    tb.dcache.m_tag_array.DEBUG_visualize_array(0,10);
    //TODO: How to represent "time"?
    //TODO: How to serialize test event and construct the interface to push test event in
}