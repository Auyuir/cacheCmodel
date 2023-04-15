#include "l1_data_cache.h"
#include <iostream>
#include <string>
#include <fstream>

struct DEBUG_L2_memRsp :public pipe_reg_base, public cache_building_block{
    DEBUG_L2_memRsp(){}
    DEBUG_L2_memRsp(L2_2_dcache_memRsp inf, enum TL_UH_D_opcode opcode)
        :m_inf(inf),m_opcode(opcode){
            set_valid();
        }

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
            std::cout << std::setw(5) << time << " | memReq";
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
        //DEBUG_init_stimuli();
    }

    void DEBUG_print_coreRsp_pop(cycle_t time){
        if (dcache.m_coreRsp_Q.m_Q.size() != 0){
            dcache.m_coreRsp_Q.DEBUG_print(time);
            dcache.m_coreRsp_Q.m_Q.pop_front();
        }
        //dcache.m_coreRsp_ready = true;
    }

    void DEBUG_cycle(cycle_t time, std::ifstream& instr_file_name){
        std::string instruction;

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
        if(!dcache.m_coreReq.is_valid() && !instr_file_name.eof()){
            getline(instr_file_name, instruction);
            //auto size = coreReq_stimuli.size();
            dcache.m_coreReq.update_with(parse_instruction(instruction));
            //coreReq_stimuli.pop_front();
        }
    }

    LSU_2_dcache_coreReq parse_instruction(std::string instruction){
        int a = 0;
        std::array<u_int32_t,32> p_addr = {};
        std::array<bool,32> p_mask = {true};
        if (instruction == "dead"){
            a = 1;
        }
            LSU_2_dcache_coreReq coreReq = LSU_2_dcache_coreReq(Read,0,random(0,31),2*a,0x1f,p_addr,p_mask);
        return coreReq;
    }

    //TODO 丰富这个函数的个数，创造更多测试
    /*void DEBUG_init_stimuli(){
        std::array<u_int32_t,32> p_addr = {};
        std::array<bool,32> p_mask = {true};
        for(int i=0;i<5;++i){
            //cache大小目前是32*2=64个line，block_idx不要超过64=0x3F
            LSU_2_dcache_coreReq coreReq=LSU_2_dcache_coreReq(Read,0,random(0,31),2*i,0x1f,p_addr,p_mask);
            coreReq_stimuli.push_back(coreReq);
            coreReq=LSU_2_dcache_coreReq(Write,0,random(0,31),2*i+1,0x1f,p_addr,p_mask);
            coreReq_stimuli.push_back(coreReq);
        }
    }*/

    l1_data_cache dcache;
private:
    //std::deque<LSU_2_dcache_coreReq> coreReq_stimuli;
    DEBUG_L2_model L2;
};

int main(int argc, char *argv[]) {

    if (argc < 2) { // 检查命令行参数是否正确
        std::cerr << "Usage: " << argv[0] << " input_file" << std::endl;
        return 1;
    }

    std::ifstream infile(argv[1]);
    if (!infile) { // 检查文件是否打开成功
        std::cerr << "Error: cannot open file " << argv[1] << std::endl;
        return 1;
    }

    std::cout << "modeling cache now" << std::endl;
    test_env tb;
    tb.dcache.m_tag_array.DEBUG_random_initialize(100);
    tb.dcache.m_tag_array.DEBUG_visualize_array(28,4);

    std::cout << std::endl << " time | event" << std::endl;
    for (int i = 100 ; i < 120 ; ++i){
        tb.DEBUG_cycle(i,infile);
    }
    
    tb.dcache.m_tag_array.DEBUG_visualize_array(28,4);

    infile.close();
    return 0;
}