#include "parameter.h"
#include "tag_array.h"
#include "miss_status_holding_reg.h"
#include "interfaces.h"

/*
class data_array : cache_building_block {//TODO: embody DATA_SRAM_LATENCY
public:
    data_array(coreRsp_Q* coreRsp_Q_ptr){
        m_coreRsp_Q_ptr = coreRsp_Q_ptr;
    }

    //this enum is not hardware-related
    enum data_array_access_type{
        READ_HIT,
        WRITE_HIT,
        MISS_RSP
    };

    struct data_array_req{
        //bool m_is_write;
        u_int32_t m_cache_req_id;
        std::array<bool,NLANE> m_mask;
        enum data_array_access_type m_type;
    };

    void cycle(){
        if(m_req != NULL){
            if(m_req->m_type==READ_HIT){
                //read();
            }
        }
    }

    void read(u_int32_t cache_req_id,std::array<bool,NLANE> mask){//Cat(set_idx,way_idx)
        dcache_2_LSU_coreRsp cRsp = dcache_2_LSU_coreRsp(cache_req_id,mask,true);
        m_coreRsp_Q_ptr->m_Q.push_back(cRsp);
    }

    void write_by_hit(u_int32_t cache_req_id,std::array<bool,NLANE> mask){
        dcache_2_LSU_coreRsp cRsp = dcache_2_LSU_coreRsp(cache_req_id,mask,false);
        m_coreRsp_Q_ptr->m_Q.push_back(cRsp);
    }

    private:
    data_array_req* m_req;
    coreRsp_Q* m_coreRsp_Q_ptr;

};*/
class l1_data_cache : public cache_building_block{
public:
    l1_data_cache(){
        m_coreReq_ptr=NULL;
        m_mshr = mshr(m_coreRsp_Q, m_memReq_Q);
    }
    
    void cycle(cycle_t time);

    //cache主体周期，产生道路分歧
    void pipe2_cycle(cycle_t time);

    //coreReq-coreRsp hit时，从data SRAM到coreRsp_Q
    void pipe3_cycle();

    void coreRsp_Q_cycle(cycle_t time);

    void memReq_Q_cycle(cycle_t time);

    void cast_amo_LSU_type_2_TLUH_param(enum LSU_cache_coreReq_type_amo coreReq_type, 
    enum TL_UH_A_opcode& TL_opcode, u_int32_t& TL_param){
        //TODO
        switch(coreReq_type)
        {
            case amoadd:
                TL_opcode = ArithmeticData;
                TL_param = u_int32_t(ADD);
                break;
            case amoxor:
                TL_opcode = LogicalData;
                TL_param = u_int32_t(XOR);
                break;
            case amoand:
                TL_opcode = LogicalData;
                TL_param = u_int32_t(AND);
                break;
            case amoor:
                TL_opcode = LogicalData;
                TL_param = u_int32_t(OR);
                break;
            case amomin:
                TL_opcode = ArithmeticData;
                TL_param = u_int32_t(MIN);
                break;
            case amomax:
                TL_opcode = ArithmeticData;
                TL_param = u_int32_t(MAX);
                break;
            case amominu:
                TL_opcode = ArithmeticData;
                TL_param = u_int32_t(MINU);
                break;
            case amomaxu:
                TL_opcode = ArithmeticData;
                TL_param = u_int32_t(MAXU);
                break;
            case amoswap:
                TL_opcode = LogicalData;
                TL_param = u_int32_t(SWAP);
                break;
        }
    }

public:
    LSU_2_dcache_coreReq* m_coreReq_ptr;
    LSU_2_dcache_coreReq* m_coreReq_pipe1_reg_ptr;//pipe1和2之间的流水线寄存器
    //TODO:[初版建模完成后]m_coreReq_pipe1_reg_ptr不用和coreReq相同的类型，而是定制化
    dcache_2_LSU_coreRsp* m_hit_pipe2_reg_ptr;//coreReq-coreRsp hit 路径上，pipe2和3之间的流水线寄存器
    bool m_coreRsp_ready;//每个周期可由tb改变的信号，代表当前周期是否可以传输coreRsp
    bool m_memReq_ready;//每个周期可由tb改变的信号，代表当前周期是否可以传输memReq

    tag_array m_tag_array;
    mshr m_mshr;
    coreRsp_Q m_coreRsp_Q;
    memReq_Q m_memReq_Q;
};

void l1_data_cache::pipe2_cycle(cycle_t time){
    auto& pipe1_r_ptr = m_coreReq_pipe1_reg_ptr;
    if(pipe1_r_ptr != nullptr){
        auto const pipe1_opcode = pipe1_r_ptr->m_opcode;
        auto const pipe1_block_idx = pipe1_r_ptr->m_block_idx;
        if (pipe1_opcode==Read || pipe1_opcode==Write || pipe1_opcode==Amo){
            if(pipe1_r_ptr->m_type == 1 ||//LR/SC
            pipe1_opcode==Amo){
                if (m_mshr.probe_spe() == AVAIL){
                    if (!m_memReq_Q.is_full()){
                        enum entry_target_type new_spe_type;
                        enum TL_UH_A_opcode new_mReq_opcode;
                        u_int32_t new_mReq_param;
                        //push spe MSHR
                        if(pipe1_opcode==Amo){
                            new_spe_type = AMO;
                            cast_amo_LSU_type_2_TLUH_param(pipe1_r_ptr->m_amo_type,
                                new_mReq_opcode,new_mReq_param);
                        }
                        else if (pipe1_opcode == Read){
                            new_spe_type = LOAD_RESRV;
                            new_mReq_opcode = Get;
                            new_mReq_param = 0x1;
                        }
                        else{
                            new_spe_type = STORE_COND;
                            new_mReq_opcode = PutFullData;
                            new_mReq_param = 0x1;
                        }
                        m_mshr.allocate_special(new_spe_type, 
                            pipe1_r_ptr->m_reg_idxw, pipe1_r_ptr->m_wid);
                        //push memReq_Q
                        dcache_2_L2_memReq new_spe_req = dcache_2_L2_memReq(
                            new_mReq_opcode, new_mReq_param, 
                            pipe1_r_ptr->m_reg_idxw, pipe1_block_idx);
                        m_memReq_Q.m_Q.push_back(new_spe_req);

                        pipe1_r_ptr = nullptr;
                    }
                }
            }else{//regular R/W
                u_int32_t way_idx=0;//hit情况下所在的way
                enum tag_access_status status = 
                    m_tag_array.probe(pipe1_block_idx,way_idx);
                if (status == HIT){
                    if(!m_coreRsp_Q.is_full()){
                        m_tag_array.read_hit_update_access_time(pipe1_block_idx,way_idx,time);
                        assert(m_hit_pipe2_reg_ptr==nullptr);
                        //本模型不建模访问data SRAM行为，在此处对该SRAM发起访问，
                        dcache_2_LSU_coreRsp read_hit_coreRsp(pipe1_r_ptr->m_reg_idxw,
                            true,pipe1_r_ptr->m_wid,pipe1_r_ptr->m_mask);
                        m_hit_pipe2_reg_ptr = &read_hit_coreRsp;//TODO:有内存管理问题吗
                        pipe1_r_ptr = nullptr;
                    }
                }else{//status == MISS
                    enum vec_mshr_status mshr_status = m_mshr.probe_vec(pipe1_block_idx);
                    if (mshr_status == PRIMARY_AVAIL){
                        if (!m_memReq_Q.is_full()){
                            //TODO push memReq_Q
                            vec_subentry new_vec_sub = vec_subentry(
                                pipe1_r_ptr->m_reg_idxw, pipe1_r_ptr->m_wid, pipe1_r_ptr->m_mask);
                            m_mshr.allocate_vec_main(pipe1_block_idx, new_vec_sub);
                            // **** TODO ****
                            pipe1_r_ptr = nullptr;
                        }
                    }else if(mshr_status == SECONDARY_AVAIL){
                        vec_subentry new_vec_sub = vec_subentry(
                            pipe1_r_ptr->m_reg_idxw, pipe1_r_ptr->m_wid, pipe1_r_ptr->m_mask);
                        m_mshr.allocate_vec_sub(pipe1_block_idx, new_vec_sub);
                        // **** TODO ****
                        pipe1_r_ptr = nullptr;
                    }//PRIMARY_FULL和SECONDARY_FULL直接跳过
                }
            }
        }
    }

    //不清除pipe_r_ptr，下个周期再处理一回
}

void l1_data_cache::pipe3_cycle(){
    if(m_hit_pipe2_reg_ptr != nullptr){
        if(!m_coreRsp_Q.is_full()){
            m_coreRsp_Q.m_Q.push_back(*m_hit_pipe2_reg_ptr);
            m_hit_pipe2_reg_ptr == nullptr;
        }
    }
    return;
}

/*所有可操作的对象包括m_coreRsp_Q, m_mshr.m_miss_req_ptr
*/
//TODO：把这个变成pipe1_cycle
void l1_data_cache::cycle(cycle_t time){

    bool mshr_2_coreRsp = false;
    //m_mshr.cycle_in(mshr_2_coreRsp, time);

    //deal with coreReq
    if (m_coreReq_ptr == nullptr){
        //jump out if-else
    }else{
        if (m_coreReq_ptr->m_opcode==Read || m_coreReq_ptr->m_opcode==Write){
            assert(m_coreReq_ptr->m_type == 0 || m_coreReq_ptr->m_type == 1);
            if(m_coreReq_ptr->m_type == 0){//TODO
                //Regular READ or WRITE
                u_int32_t way_idx=0;
                enum tag_access_status status = m_tag_array.probe(m_coreReq_ptr->m_block_idx,way_idx);
                if (status == HIT){
                    if (m_coreRsp_Q.m_Q.size() < CORE_RSP_Q_DEPTH){
                        m_tag_array.read_hit_update_access_time(m_coreReq_ptr->m_block_idx,way_idx,time);
                        dcache_2_LSU_coreRsp read_hit_coreRsp(m_coreReq_ptr->m_reg_idxw,true,m_coreReq_ptr->m_wid,m_coreReq_ptr->m_mask);
                        m_coreRsp_Q.m_Q.push_back(read_hit_coreRsp);
                        m_coreReq_ptr = nullptr;
                    }//else 不清空coreReq，下个周期重新判断一次hit/miss
                }
            }/*else{//m_coreReq_ptr->m_type == 1
                if (m_mshr.m_miss_req_ptr == nullptr){
                        vec_subentry lrsc_sub = vec_subentry(
                            m_coreReq_ptr->m_reg_idxw,
                            m_coreReq_ptr->m_wid,
                            m_coreReq_ptr->m_mask);//TODO 以后换成SCALAR_MASK
                        mshr_miss_req_t new_lrsc_mshr_req;
                        if (m_coreReq_ptr->m_opcode == Read){
                            new_lrsc_mshr_req = mshr_miss_req_t(
                                m_coreReq_ptr->m_block_idx,
                                LOAD_RESRV, lrsc_sub);
                        }else if (m_coreReq_ptr->m_opcode == Write){
                            new_lrsc_mshr_req = mshr_miss_req_t(
                                m_coreReq_ptr->m_block_idx,
                                STORE_COND, lrsc_sub);
                        }
                        m_mshr.m_miss_req_ptr = &new_lrsc_mshr_req;
                        m_coreReq_ptr = nullptr;
                }
            }*/
        }
        else if(m_coreReq_ptr->m_opcode==Fence){
            //Fence
        }else {
            assert(m_coreReq_ptr->m_opcode==Amo);
            //AMO
        }
    }
}

//需要tb先取出当前coreRsp里front的内容之后，再运行该函数
void l1_data_cache::coreRsp_Q_cycle(cycle_t time){
    if(m_coreRsp_ready){
        m_coreRsp_Q.m_Q.pop_front();
    }
    return;
}

//需要tb先取出当前memReq_Q里front的内容之后，再运行该函数
void l1_data_cache::memReq_Q_cycle(cycle_t time){
    if(m_memReq_ready){
        m_memReq_Q.m_Q.pop_front();
    }
    return;
}

int main() {
    std::cout << "modeling cache tag array now" << std::endl;
    l1_data_cache dcache;
    dcache.m_tag_array.DEBUG_random_initialize(100);
    //initialize a coreReq
    std::array<u_int32_t,32> p_addr = {};
    std::array<bool,32> p_mask = {true};
    LSU_2_dcache_coreReq coreReq=LSU_2_dcache_coreReq(Read,0,0,1,0xff02,p_addr,p_mask);
    auto temp = &coreReq;
    dcache.m_coreReq_ptr = temp;
    dcache.cycle(101);
    dcache.cycle(102);
    std::cout << dcache.get_tag(0xffff) <<std::endl;

    dcache.m_tag_array.DEBUG_visualize_array(0,10);
    //TODO: How to represent "time"?
    //TODO: How to serialize test event and construct the interface to push test event in
}