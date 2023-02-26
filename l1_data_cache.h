#include "parameter.h"
#include "tag_array.h"
#include "miss_status_holding_reg.h"
#include "interfaces.h"

class mshr_missRsp_pipe_reg : public mshr_miss_rsp, public pipe_reg_base{
    public:
    mshr_missRsp_pipe_reg(){}

    void update_with(mshr_miss_rsp miss_rsp){
        m_type = miss_rsp.m_type;
        m_req_id = miss_rsp.m_req_id;
        m_block_idx = miss_rsp.m_block_idx;
        set_valid();
    }
};

class l1_data_cache : public cache_building_block{
public:
    l1_data_cache(){}

    void coreReq_pipe1_cycle(cycle_t time);

    //cache主体周期，产生道路分歧
    void coreReq_pipe2_cycle(cycle_t time);

    //coreReq-coreRsp hit时，从data SRAM到coreRsp_Q
    void coreReq_pipe3_cycle();

    void memRsp_pipe1_cycle(cycle_t time);

    void memRsp_pipe2_cycle(cycle_t time);

    void cycle(cycle_t time);

    void cast_amo_LSU_type_2_TLUH_param(enum LSU_cache_coreReq_type_amo coreReq_type, 
    enum TL_UH_A_opcode& TL_opcode, u_int32_t& TL_param){
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
    coreReq_pipe_reg m_coreReq;
    coreReq_pipe_reg m_coreReq_pipe1_reg;//pipe1和2之间的流水线寄存器
    //TODO:[初版建模完成后]m_coreReq_pipe1_reg_ptr不用和coreReq相同的类型，而是定制化
    coreRsp_pipe_reg m_coreRsp_pipe2_reg;//read hit 路径/write miss路径/missRsp路径
    //该寄存器由l1_data_cache的memRsp_pipe1_cycle检查和置1，由memRsp_pipe2_cycle置0。
    //如果用SRAM实现MSHR，硬件中没有这个寄存器，用SRAM的保持功能实现相应功能。
    //如果用reg实现MSHR，可以按照本模型行为设计寄存器。
    mshr_missRsp_pipe_reg m_memRsp_pipe1_reg;
    bool tag_req_current_missRsp_has_sent = false;//m_memRsp_pipe1_reg的一部分，单独控制信号

    tag_array m_tag_array;
    mshr m_mshr;
    coreRsp_Q m_coreRsp_Q;
    memReq_Q m_memReq_Q;
    memRsp_Q m_memRsp_Q;
};

void l1_data_cache::coreReq_pipe1_cycle(cycle_t time){
    if(m_memRsp_Q.m_Q.size() == 0){
        if(m_coreReq.is_valid()){
            if(!m_coreReq_pipe1_reg.is_valid()){
                auto const coreReq_opcode = m_coreReq.m_opcode;
                assert(coreReq_opcode<=3);
                //debug info
                std::cout << std::setw(5) << time << " | coreReq";
                std::cout << ", reg_idx" << m_coreReq.m_reg_idxw ;
                std::cout << ", opcode=" << coreReq_opcode <<std::endl;

                if (coreReq_opcode==Read || coreReq_opcode==Write || coreReq_opcode==Amo){
                    if(m_coreReq.m_type == 1 || coreReq_opcode==Amo){//LR/SC
                        //发起对speMSHR可用性的检查
                    }else{//regular R/W
                        //发起tag probe
                        //同步发起vecMSHR probe
                    }
                }//else: flush or invalidate
                m_coreReq_pipe1_reg = m_coreReq;
                m_coreReq.invalidate();
            }
        }
    }
}

void l1_data_cache::coreReq_pipe2_cycle(cycle_t time){
    auto& pipe1_r = m_coreReq_pipe1_reg;
    if(pipe1_r.is_valid() && !m_memRsp_pipe1_reg.is_valid()){
        auto const pipe1_opcode = pipe1_r.m_opcode;
        auto const pipe1_block_idx = pipe1_r.m_block_idx;
        if (pipe1_opcode==Read || pipe1_opcode==Write || pipe1_opcode==Amo){
            if(pipe1_r.m_type == 1 ||//LR/SC
            pipe1_opcode==Amo){
                //实际硬件行为中，mshr的probe发生在pipe1_cycle，结果在pipe2_cycle取得。
                if (m_mshr.probe_spe() == AVAIL){
                    if (!m_memReq_Q.is_full()){
                        enum entry_target_type new_spe_type;
                        enum TL_UH_A_opcode new_mReq_opcode;
                        u_int32_t new_mReq_param;
                        //push spe MSHR
                        if(pipe1_opcode==Amo){
                            new_spe_type = AMO;
                            cast_amo_LSU_type_2_TLUH_param(pipe1_r.m_amo_type,
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
                            pipe1_r.m_reg_idxw, pipe1_r.m_wid);
                        //push memReq_Q
                        dcache_2_L2_memReq new_spe_req = dcache_2_L2_memReq(
                            new_mReq_opcode, new_mReq_param, 
                            pipe1_r.m_reg_idxw, pipe1_block_idx);
                        m_memReq_Q.m_Q.push_back(new_spe_req);

                        pipe1_r.invalidate();
                    }
                }
            }else{//regular R/W
                u_int32_t way_idx=0;//hit情况下所在的way
                //实际硬件行为中，tag的probe发生在pipe1_cycle，结果在pipe2_cycle取得。
                enum tag_access_status status = 
                    m_tag_array.probe(pipe1_block_idx,way_idx);
                if (status == HIT){
                    if(!m_coreRsp_Q.is_full()){
                        auto set_idx = get_set_idx(pipe1_block_idx);
                        m_tag_array.read_hit_update_access_time(set_idx,way_idx,time);
                        assert(!m_coreRsp_pipe2_reg.is_valid());
                        //arrange coreRsp
                        //本模型不建模访问data SRAM行为，在此处对该SRAM发起访问，
                        bool rsp_with_data = (pipe1_opcode==Read);
                        dcache_2_LSU_coreRsp hit_coreRsp(pipe1_r.m_reg_idxw,
                            rsp_with_data,pipe1_r.m_wid,pipe1_r.m_mask);
                        m_coreRsp_pipe2_reg.update_with(hit_coreRsp);//TODO:有内存管理问题吗
                        if (rsp_with_data){
                            m_tag_array.write_hit_mark_dirty(way_idx,set_idx,time);
                        }
                        pipe1_r.invalidate();
                    }
                }else{//status == MISS
                    if(pipe1_opcode==Read){
                        //实际硬件行为中，mshr的probe发生在pipe1_cycle，结果在pipe2_cycle取得。
                        enum vec_mshr_status mshr_status = m_mshr.probe_vec(pipe1_block_idx);
                        if (mshr_status == PRIMARY_AVAIL){
                            if (!m_memReq_Q.is_full()){
                                //vecMSHR记录新entry
                                vec_subentry new_vec_sub = vec_subentry(
                                    pipe1_r.m_reg_idxw, pipe1_r.m_wid, pipe1_r.m_mask);
                                m_mshr.allocate_vec_main(pipe1_block_idx, new_vec_sub);
                                //push memReq Q
                                dcache_2_L2_memReq new_read_miss = dcache_2_L2_memReq(
                                    Get, 0x0, pipe1_r.m_reg_idxw, pipe1_block_idx);
                                m_memReq_Q.m_Q.push_back(new_read_miss);
                                pipe1_r.invalidate();
                            }
                        }else if(mshr_status == SECONDARY_AVAIL){
                            //vecMSHR在旧entry下记录新成员
                            vec_subentry new_vec_sub = vec_subentry(
                                pipe1_r.m_reg_idxw, pipe1_r.m_wid, pipe1_r.m_mask);
                            m_mshr.allocate_vec_sub(pipe1_block_idx, new_vec_sub);
                            pipe1_r.invalidate();
                        }//PRIMARY_FULL和SECONDARY_FULL直接跳过
                    }else{//Write (write no allocation when miss)
                        if (!m_memReq_Q.is_full() && !m_coreRsp_Q.is_full()){
                            //arrange coreRsp
                            dcache_2_LSU_coreRsp write_miss_coreRsp(pipe1_r.m_reg_idxw,
                                false,pipe1_r.m_wid,pipe1_r.m_mask);
                            m_coreRsp_pipe2_reg.update_with(write_miss_coreRsp);
                            //push memReq Q
                            dcache_2_L2_memReq new_write_miss = dcache_2_L2_memReq(
                                PutFullData, 0x0, pipe1_r.m_reg_idxw, pipe1_block_idx);
                            m_memReq_Q.m_Q.push_back(new_write_miss);
                            pipe1_r.invalidate();
                        }
                    }
                }
            }
        }else{//pipe1_opcode==InvOrFlu
            int set_idx;
            int way_idx;
            if(m_tag_array.has_dirty(set_idx,way_idx)){
                m_tag_array.flush_one(m_memReq_Q,set_idx,way_idx);
            }else{
                if(pipe1_r.m_type == 1){//Invalidate
                    if(m_mshr.empty()){
                        m_tag_array.invalidate_all();
                        if(!m_coreRsp_Q.is_full()){
                            dcache_2_LSU_coreRsp Invalidate_coreRsp(pipe1_r.m_reg_idxw,
                                false,pipe1_r.m_wid,pipe1_r.m_mask);
                            m_coreRsp_pipe2_reg.update_with(Invalidate_coreRsp);
                            pipe1_r.invalidate();
                        }
                    }
                }else{//Flush
                    if(!m_coreRsp_Q.is_full()){
                        dcache_2_LSU_coreRsp Flush_coreRsp(pipe1_r.m_reg_idxw,
                            false,pipe1_r.m_wid,pipe1_r.m_mask);
                        m_coreRsp_pipe2_reg.update_with(Flush_coreRsp);
                        pipe1_r.invalidate();
                    }
                }
            }
        }
    }
    //不清除pipe_r_ptr，下个周期再处理一回
}

void l1_data_cache::coreReq_pipe3_cycle(){
    if(m_coreRsp_pipe2_reg.is_valid()){
        if(!m_coreRsp_Q.is_full()){
            m_coreRsp_Q.m_Q.push_back(m_coreRsp_pipe2_reg);
            m_coreRsp_pipe2_reg.invalidate();
        }
    }
}

void l1_data_cache::memRsp_pipe1_cycle(cycle_t time){
    if(m_memRsp_Q.m_Q.size() != 0){
        if(!m_memRsp_pipe1_reg.is_valid()){
            auto const req_id = m_memRsp_Q.m_Q.front().m_req_id;
            block_addr_t block_idx;
            auto missRsp_type = m_mshr.detect_missRsp_type(block_idx, req_id);
            mshr_miss_rsp new_miss_rsp = mshr_miss_rsp(missRsp_type,req_id, block_idx);
            m_memRsp_pipe1_reg.update_with(new_miss_rsp);

            //debug info
            std::cout<< std::setw(5) << time << " | memRsp";
            std::cout << ", req_id=" << req_id <<std::endl;

            m_memRsp_Q.m_Q.pop_front();
        }
    }
}

void l1_data_cache::memRsp_pipe2_cycle(cycle_t time){
    if(m_memRsp_pipe1_reg.is_valid()){
        auto& type = m_memRsp_pipe1_reg.m_type;
        auto& block_idx = m_memRsp_pipe1_reg.m_block_idx;
        auto& req_id = m_memRsp_pipe1_reg.m_req_id;
        bool current_missRsp_clear = false;
        if (type == REGULAR_READ_MISS){
            bool allocate_success = true;
            if(!tag_req_current_missRsp_has_sent){
                allocate_success = m_tag_array.allocate(m_memReq_Q, block_idx, time);
                tag_req_current_missRsp_has_sent = true;
                //本建模不体现，硬件在这里需要启动data SRAM的更新
            }
            if(!m_mshr.current_main_0_sub(block_idx)){
                if(!m_coreRsp_pipe2_reg.is_valid()){
                    bool main_finish = m_mshr.vec_arrange_core_rsp(
                        m_coreRsp_pipe2_reg, block_idx);
                    if(main_finish){
                        tag_req_current_missRsp_has_sent = !allocate_success;
                        current_missRsp_clear = allocate_success;
                    }
                }
            }else{
                //本建模中vec_entry不会为0，因为没有建模data SRAM的多周期写入行为
                //所以这条路径不会被触发
                tag_req_current_missRsp_has_sent = !allocate_success;
                current_missRsp_clear = allocate_success;
            }
        }else{//AMO/LR/SC
            if(!m_coreRsp_pipe2_reg.is_valid()){
                m_mshr.special_arrange_core_rsp(m_coreRsp_pipe2_reg, req_id);
                current_missRsp_clear = true;
            }
        }

        if(current_missRsp_clear){
            m_memRsp_pipe1_reg.invalidate();
        }
    }
}

void l1_data_cache::cycle(cycle_t time){

    coreReq_pipe3_cycle();

    coreReq_pipe2_cycle(time);//coreReq pipe2必须在memRsp之前，因为memRsp优先级高
    memRsp_pipe2_cycle(time);

    coreReq_pipe1_cycle(time);//memRsp的优先级比coreReq高，coreReq pipe1必须在memRsp pipe1之前运行
    memRsp_pipe1_cycle(time);//否则memRsp pipe1的运行会pop memRsp_Q
}
