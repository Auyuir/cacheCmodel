#ifndef L1_DATA_CACHE_H
#define L1_DATA_CACHE_H

#include "parameter.h"
#include "tag_array.h"
#include "miss_status_holding_reg.h"
#include "data_array.h"
#include "interfaces.h"
#include <fstream>

class mshr_missRsp_pipe_reg : public mshr_miss_rsp, public pipe_reg_base{
    public:
    mshr_missRsp_pipe_reg(){}

    void update_with(mshr_miss_rsp miss_rsp, cache_line_t& fill_data){
        m_type = miss_rsp.m_type;
        m_req_id = miss_rsp.m_req_id;
        m_block_idx = miss_rsp.m_block_idx;
        m_fill_data = fill_data;
        set_valid();
    }

    cache_line_t m_fill_data;
};

class l1_data_cache : public cache_building_block{
public:
    l1_data_cache(){};
    l1_data_cache(int verbose_level):m_DEBUG_verbose_level(verbose_level){};

    void coreReq_pipe0_cycle(cycle_t time);

    //cache主体周期，产生道路分歧
    void coreReq_pipe1_cycle(cycle_t time);

    //coreReq-coreRsp hit时，从data SRAM到coreRsp_Q
    void coreRsp_pipe2_cycle();

    void memRsp_pipe0_cycle(cycle_t time);

    void memRsp_pipe1_cycle(cycle_t time);

    void memReq_pipe2_cycle();

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

    void DEBUG_waveform_a_cycle(std::ofstream& waveform_file){
        DEBUG_waveform_pipe_reg(waveform_file);
        DEBUG_waveform_coreReq_a_cycle(waveform_file);
        DEBUG_waveform_coreRsp_a_cycle(waveform_file);
        DEBUG_waveform_memRsp_a_cycle(waveform_file);
        DEBUG_waveform_memReq_a_cycle(waveform_file);
    }

    void DEBUG_waveform_pipe_reg(std::ofstream& waveform_file){
        waveform_file << m_coreReq_pipe1_reg.is_valid() << "," << m_coreReq_pipe1_reg.m_wid << ",";
        waveform_file << m_coreRsp_pipe2_reg.is_valid() << "," << m_coreRsp_pipe2_reg.m_wid << ",";
        waveform_file << m_memRsp_pipe1_reg.is_valid() << "," << m_memRsp_pipe1_reg.m_req_id << ",";
    }

    void DEBUG_waveform_coreReq_a_cycle(std::ofstream& waveform_file){
        auto& o = m_coreReq;
        waveform_file << o.is_valid() << "," << o.m_opcode << "," << o.m_type << "," << o.m_wid << "," ;
        waveform_file << o.m_reg_idxw << "," << o.m_block_idx << "," << o.m_block_offset[0] << ",";
        waveform_file << o.m_mask[0] << "," << o.m_mask[1] << "," << o.m_data[0] << ",";
    }

    void DEBUG_waveform_coreRsp_a_cycle(std::ofstream& waveform_file){
        auto& o = m_coreRsp_Q.m_Q.front();
        waveform_file << !m_coreRsp_Q.is_empty() << "," << o.m_wid << "," << o.m_reg_idxw << "," << o.m_mask[0] << ",";
        waveform_file << (o.m_mask[0] && !o.m_mask[1]) << "," << o.m_data[0] << ",";
    }

    void DEBUG_waveform_memRsp_a_cycle(std::ofstream& waveform_file){
        if(m_memRsp_Q.is_empty()){
            waveform_file << "x,x,x,x,x,x,";
        }else{
            auto& o = m_memRsp_Q.m_Q.back();
            waveform_file << !m_memRsp_Q.is_empty() << "," << o.d_opcode << "," << o.d_source << ",";
            waveform_file << o.d_mask[0] << "," << o.d_mask[1] << "," << o.d_data[0] << ",";    
        }
    }

    void DEBUG_waveform_memReq_a_cycle(std::ofstream& waveform_file){
        auto& o = m_memReq_Q.m_Q.front();
        waveform_file << !m_memReq_Q.is_empty() << "," << o.a_opcode << "," << o.a_param << "," << o.a_source << ",";
        waveform_file << o.a_address << "," << o.a_mask[0] << "," << o.a_mask[1] << "," << o.a_data[0];
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
    memReq_pipe_reg m_memReq_pipe3_reg;//m_memReq_Q出队有效且无需WSHR、MSHR保护时为真，组合逻辑

    tag_array m_tag_array;
    mshr m_mshr;
    data_array m_data_array;
    coreRsp_Q m_coreRsp_Q;
    memReq_Q m_memReq_Q;
    memRsp_Q m_memRsp_Q;

private:
    int m_DEBUG_verbose_level=1;
};

void l1_data_cache::coreReq_pipe0_cycle(cycle_t time){
    if(m_memRsp_Q.m_Q.size() == 0){
        if(m_coreReq.is_valid()){
            if(!m_coreReq_pipe1_reg.is_valid()){
                auto const coreReq_opcode = m_coreReq.m_opcode;
                assert(coreReq_opcode<=3);
                //debug info
                if(m_DEBUG_verbose_level>=1){
                    std::cout << std::setw(5) << time << " | coreReq";
                    std::cout << ", reg_idx" << m_coreReq.m_reg_idxw ;
                    std::cout << ", opcode=" << coreReq_opcode <<std::endl;
                }
                if (coreReq_opcode==Read || coreReq_opcode==Write || coreReq_opcode==Amo){
                    if(m_coreReq.m_type == 1 || coreReq_opcode==Amo){//LR/SC
                        m_mshr.probe_spe_in(coreReq_opcode==Write);//输入是“is_store_conditional”
                    }else{//regular R/W
                        m_tag_array.probe_in(m_coreReq.m_block_idx);
                        m_mshr.probe_vec_in(m_coreReq.m_block_idx);
                    }
                }//else: flush or invalidate
                m_coreReq_pipe1_reg = m_coreReq;
                m_coreReq.invalidate();
            }
        }
    }
}

void l1_data_cache::coreReq_pipe1_cycle(cycle_t time){
    auto& pipe1_r = m_coreReq_pipe1_reg;
    if(pipe1_r.is_valid() && !m_memRsp_pipe1_reg.is_valid()){
        auto const pipe1_opcode = pipe1_r.m_opcode;
        auto const pipe1_block_idx = pipe1_r.m_block_idx;
        if (pipe1_opcode==Read || pipe1_opcode==Write || pipe1_opcode==Amo){
            if(pipe1_r.m_type == 1 ||//LR/SC
            pipe1_opcode==Amo){
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
                //实际硬件行为中，mshr的probe发生在pipe1_cycle，结果在pipe2_cycle取得。
                if (m_mshr.probe_spe_out() == AVAIL){
                    u_int32_t tag_evict;
                    u_int32_t way_evict;
                    const u_int32_t set_idx = get_set_idx(pipe1_block_idx);
                    bool dirty = m_tag_array.line_is_dirty(pipe1_block_idx,way_evict);
                    if (!m_memReq_Q.is_full()){
                        if (dirty){
                            m_tag_array.flush_one(set_idx,way_evict);
                            u_int32_t block_addr = (tag_evict << LOGB2(NSET) + set_idx) << 2;
                            std::array<bool,LINEWORDS> full_mask;
                            full_mask.fill(true);
                            dcache_2_L2_memReq new_dirty_back = dcache_2_L2_memReq(
                                PutFullData, 
                                0x0, 
                                0xFFFFF, //TODO这里要加WSHR
                                block_addr,
                                m_data_array.read(set_idx,way_evict),//TODO这里data_array不能在这个周期完成
                                full_mask);
                            m_memReq_Q.m_Q.push_back(new_dirty_back);
                        }else{
                            m_mshr.allocate_special(new_spe_type, 
                            pipe1_r.m_reg_idxw, pipe1_r.m_wid);
                            //push memReq_Q
                            cache_line_t data_memReq{pipe1_r.m_data[0]};
                            std::array<bool,LINEWORDS> onehot_mask{true};
                            dcache_2_L2_memReq new_spe_req = dcache_2_L2_memReq(
                                new_mReq_opcode,
                                new_mReq_param,
                                pipe1_r.m_reg_idxw,
                                pipe1_block_idx,
                                data_memReq,
                                onehot_mask);
                            m_memReq_Q.m_Q.push_back(new_spe_req);
                            m_tag_array.invalidate_chosen(set_idx,way_evict);
                            pipe1_r.invalidate();
                        }
                        //这里在建模中一个周期向memReqQ入栈了两个值，后面还得再考虑考虑
                    }
                }
            }else{//regular R/W
                u_int32_t way_idx=0;//hit情况下所在的way
                //实际硬件行为中，tag的probe发生在pipe1_cycle，结果在pipe2_cycle取得。
                enum tag_access_status status = m_tag_array.probe_out(way_idx);
                if (status == HIT){
                    if(!m_coreRsp_Q.is_full()){
                        u_int32_t set_idx = get_set_idx(pipe1_block_idx);
                        m_tag_array.read_hit_update_access_time(set_idx,way_idx,time);
                        assert(!m_coreRsp_pipe2_reg.is_valid());
                        //arrange coreRsp
                        vec_nlane_t data{0};
                        if (pipe1_opcode==Write){
                            m_tag_array.write_hit_mark_dirty(way_idx,set_idx,time);
                            m_data_array.write_hit(set_idx,way_idx,pipe1_r.m_data,pipe1_r.m_block_offset,pipe1_r.m_mask);
                        }else{
                            auto data_from_array = m_data_array.read(set_idx,way_idx);
                            for(int i = 0;i<NLANE;++i){
                                if(pipe1_r.m_mask[i]==true){//mem order to core order crossbar
                                    data[i] = data_from_array[pipe1_r.m_block_offset[i]];
                                }
                            }
                        }
                        dcache_2_LSU_coreRsp hit_coreRsp(pipe1_r.m_reg_idxw,
                            data, 
                            pipe1_r.m_wid, 
                            pipe1_r.m_mask);
                        m_coreRsp_pipe2_reg.update_with(hit_coreRsp);
                        pipe1_r.invalidate();
                    }
                }else{//
                    assert((status == MISS) && "cReq st1 tag状态既不H也不M");
                    if(pipe1_opcode==Read){
                        //实际硬件行为中，mshr的probe发生在pipe1_cycle，结果在pipe2_cycle取得。
                        enum vec_mshr_status mshr_status = m_mshr.probe_vec_out();
                        if (mshr_status == PRIMARY_AVAIL){
                            if (!m_memReq_Q.is_full()){
                                //vecMSHR记录新entry
                                vec_subentry new_vec_sub = vec_subentry(
                                    pipe1_r.m_reg_idxw,
                                    pipe1_r.m_wid,
                                    pipe1_r.m_mask,
                                    pipe1_r.m_block_offset);
                                m_mshr.allocate_vec_main(pipe1_block_idx, new_vec_sub);
                                //push memReq Q
                                cache_line_t data{0};
                                std::array<bool,LINEWORDS> full_mask;
                                full_mask.fill(true);
                                dcache_2_L2_memReq new_read_miss = dcache_2_L2_memReq(
                                    Get,
                                    0x0,
                                    pipe1_r.m_reg_idxw,
                                    pipe1_block_idx,
                                    data,
                                    full_mask);
                                m_memReq_Q.m_Q.push_back(new_read_miss);
                                pipe1_r.invalidate();
                            }
                        }else if(mshr_status == SECONDARY_AVAIL){
                            //vecMSHR在旧entry下记录新成员
                            vec_subentry new_vec_sub = vec_subentry(
                                pipe1_r.m_reg_idxw,
                                pipe1_r.m_wid,
                                pipe1_r.m_mask,
                                pipe1_r.m_block_offset);
                            m_mshr.allocate_vec_sub(pipe1_block_idx, new_vec_sub);
                            pipe1_r.invalidate();
                        }//PRIMARY_FULL和SECONDARY_FULL直接跳过
                    }else{//Write (write no allocation when miss)
                        if (!m_memReq_Q.is_full()){//&& !m_coreRsp_Q.is_full()){
                            //push memReq Q
                            cache_line_t data_memReq;
                            std::array<bool,LINEWORDS> write_miss_mask;
                            for(int i = 0;i<NLANE;++i){//core order to mem order crossbar
                                if(pipe1_r.m_mask[i]==true){//在硬件中，这里是offset矩阵转置的独热码
                                    data_memReq[pipe1_r.m_block_offset[i]] = pipe1_r.m_data[i];
                                    write_miss_mask[pipe1_r.m_block_offset[i]] = true;
                                }
                            }
                            dcache_2_L2_memReq new_write_miss = dcache_2_L2_memReq(
                                PutPartialData, 
                                0x0,
                                pipe1_r.m_reg_idxw,
                                pipe1_block_idx,
                                data_memReq,
                                write_miss_mask);
                            m_memReq_Q.m_Q.push_back(new_write_miss);
                            pipe1_r.invalidate();
                        }
                    }
                }
            }
        }else if(pipe1_opcode==InvOrFlu){//pipe1_opcode==InvOrFlu
            u_int32_t set_idx;
            u_int32_t way_idx;
            u_int32_t tag_evict;
            if(pipe1_r.m_type == 2){
                if(m_mshr.empty()){
                    pipe1_r.invalidate();
                }
            }else{
                if(m_tag_array.has_dirty(tag_evict,set_idx,way_idx)){
                    if(!m_memReq_Q.is_full()){
                        m_tag_array.flush_one(set_idx,way_idx);
                        u_int32_t block_addr = (tag_evict << LOGB2(NSET) + set_idx) << 2;
                        std::array<bool,LINEWORDS> full_mask;
                        full_mask.fill(true);
                        dcache_2_L2_memReq new_dirty_back = dcache_2_L2_memReq(//TODO这里data_array不能在这个周期完成
                            PutFullData,
                            0x0,
                            0xFFFFF,
                            block_addr,
                            m_data_array.read(set_idx,way_idx),
                            full_mask);
                        m_memReq_Q.m_Q.push_back(new_dirty_back);
                    }
                }else{
                    vec_nlane_t data{0};
                    if(pipe1_r.m_type == 1){//Invalidate
                        if(m_mshr.empty()){
                            m_tag_array.invalidate_all();
                            if(!m_coreRsp_Q.is_full()){
                                dcache_2_LSU_coreRsp Invalidate_coreRsp(
                                    pipe1_r.m_reg_idxw,
                                    data,
                                    pipe1_r.m_wid,
                                    pipe1_r.m_mask);
                                m_coreRsp_pipe2_reg.update_with(Invalidate_coreRsp);
                                pipe1_r.invalidate();
                            }
                        }
                    }else if(pipe1_r.m_type == 0){//Flush
                        if(!m_coreRsp_Q.is_full()){
                            dcache_2_LSU_coreRsp Flush_coreRsp(pipe1_r.m_reg_idxw,
                                data,pipe1_r.m_wid,pipe1_r.m_mask);
                            m_coreRsp_pipe2_reg.update_with(Flush_coreRsp);
                            pipe1_r.invalidate();
                        }
                    }else{
                        assert(false && "非法InvOrFlu");
                    }
                }
            }
        }else{
            
        }
    }
    //不清除pipe_r_ptr，下个周期再处理一回
}

void l1_data_cache::coreRsp_pipe2_cycle(){
    if(m_coreRsp_pipe2_reg.is_valid()){
        if(!m_coreRsp_Q.is_full()){
            m_coreRsp_Q.m_Q.push_back(m_coreRsp_pipe2_reg);
            m_coreRsp_pipe2_reg.invalidate();
        }
    }
}

void l1_data_cache::memRsp_pipe0_cycle(cycle_t time){
    if(m_memRsp_Q.m_Q.size() != 0){
        if(!m_memRsp_pipe1_reg.is_valid()){
            if(m_memRsp_Q.m_Q.front().d_opcode == AccessAckData){
                //这种机制要求SC也返回AccessAckData，而不是AccessAck
                auto const req_id = m_memRsp_Q.m_Q.front().d_source;
                block_addr_t block_idx;
                auto missRsp_type = m_mshr.detect_missRsp_type(block_idx, req_id);
                mshr_miss_rsp new_miss_rsp = mshr_miss_rsp(missRsp_type,req_id, block_idx);
                m_memRsp_pipe1_reg.update_with(new_miss_rsp,m_memRsp_Q.m_Q.front().d_data);
            }else{
                //在这更新WSHR
            }
            if(m_DEBUG_verbose_level>=1){
                std::cout<< std::setw(5) << time << " | memRsp";
                std::cout << ", req_id=" << m_memRsp_Q.m_Q.front().d_source <<std::endl;
            }
            m_memRsp_Q.m_Q.pop_front();
        }
    }
}

void l1_data_cache::memRsp_pipe1_cycle(cycle_t time){
    if(m_memRsp_pipe1_reg.is_valid()){
        auto& type = m_memRsp_pipe1_reg.m_type;
        auto& block_idx = m_memRsp_pipe1_reg.m_block_idx;
        u_int32_t set_idx = get_set_idx(block_idx);
        u_int32_t way_replace;
        auto& req_id = m_memRsp_pipe1_reg.m_req_id;
        bool current_missRsp_clear = false;
        if (type == REGULAR_READ_MISS){
            bool allocate_success = true;
            bool need_replace = false;
            if(!tag_req_current_missRsp_has_sent){
                u_int32_t tag_replace;
                need_replace = !m_tag_array.allocate(m_memReq_Q.is_full(), 
                    tag_replace, way_replace, block_idx, time);
                allocate_success = !need_replace || (need_replace && !m_memReq_Q.is_full());
                if(allocate_success){
                    m_data_array.fill(set_idx,way_replace,m_memRsp_pipe1_reg.m_fill_data);
                    if(need_replace){
                        u_int32_t block_addr = (tag_replace << LOGB2(NSET) + set_idx) << 2;
                        std::array<bool,LINEWORDS> full_mask;
                        full_mask.fill(true);
                        dcache_2_L2_memReq new_dirty_back = dcache_2_L2_memReq(//TODO这里data_array不能在这个周期完成
                            PutFullData,
                            0x0,
                            0xFFFFF,
                            block_addr,
                            m_data_array.read(set_idx,way_replace),
                            full_mask);
                        m_memReq_Q.m_Q.push_back(new_dirty_back);
                    }
                }
                tag_req_current_missRsp_has_sent = true;
            }
            if(!m_mshr.current_main_0_sub(block_idx)){
                assert(!m_mshr.has_secondary_full_return() && "MSHR违规置SECONDARY_FULL_RETURN");
                if(!m_coreRsp_pipe2_reg.is_valid()){
                    bool main_finish = m_mshr.vec_arrange_core_rsp(
                        m_coreRsp_pipe2_reg,
                        block_idx,
                        m_memRsp_pipe1_reg.m_fill_data);
                    if(main_finish && !m_mshr.has_secondary_full_return()){
                        tag_req_current_missRsp_has_sent = !allocate_success;
                        current_missRsp_clear = allocate_success;
                    }
                }
            }else if(m_mshr.has_secondary_full_return()){
                if(!m_coreRsp_pipe2_reg.is_valid()){
                    auto& cReq_st1_r = m_coreReq_pipe1_reg;
                    auto& mRsp_st1_r = m_memRsp_pipe1_reg;
                    vec_nlane_t data;
                    for(int i = 0;i<NLANE;++i){
                        if(cReq_st1_r.m_mask[i]==true){//mem order to core order crossbar
                            data[i] = mRsp_st1_r.m_fill_data[cReq_st1_r.m_block_offset[i]];
                        }
                    }
                    dcache_2_LSU_coreRsp secondary_full_return_cRsp(
                        cReq_st1_r.m_reg_idxw,
                        data,
                        cReq_st1_r.m_wid,
                        cReq_st1_r.m_mask);
                    m_coreRsp_pipe2_reg.update_with(secondary_full_return_cRsp);
                    
                    cReq_st1_r.invalidate();
                    tag_req_current_missRsp_has_sent = !allocate_success;
                    current_missRsp_clear = allocate_success;
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

void l1_data_cache::memReq_pipe2_cycle(){
    if(!m_memReq_Q.is_empty() && !m_memReq_pipe3_reg.is_valid()){
        dcache_2_L2_memReq& mReq = m_memReq_Q.m_Q.front();
        u_int32_t mReq_block_addr = get_block_idx(mReq.a_address);
        auto& op = mReq.a_opcode;
        bool is_write = ((op == PutFullData) || (op == PutPartialData)) && (mReq.a_param == 0);
        bool is_read = (op == Get) && (mReq.a_param == 0);
        bool mshr_protect = false;
        bool wshr_protect = false;
        bool coreRsp_blocked = false;
        if(is_write){
            //wshr_protect
            mshr_protect = m_mshr.w_s_protection_check(mReq_block_addr);
            coreRsp_blocked = m_coreRsp_Q.is_full();
        }else if(is_read){
            //wshr_protect
        }
        
        if(!mshr_protect && !wshr_protect && !coreRsp_blocked){
            if(is_write){
                //arrange coreRsp
                vec_nlane_t dummy_data{0};
                std::array<bool,NLANE> dummy_mask{false};
                //在目前的建模中core不需要写入的返回，所以没有建模该请求的内容
                dcache_2_LSU_coreRsp write_miss_coreRsp(99,//regidx
                    dummy_data,
                    0,//id
                    dummy_mask);
                m_coreRsp_Q.m_Q.push_back(write_miss_coreRsp);
            }
            m_memReq_pipe3_reg.update_with(mReq);
            m_memReq_Q.m_Q.pop_front();
        }
    }
}

void l1_data_cache::cycle(cycle_t time){

    coreRsp_pipe2_cycle();
    memReq_pipe2_cycle();

    coreReq_pipe1_cycle(time);//coreReq pipe2必须在memRsp之前，因为memRsp优先级高
    memRsp_pipe1_cycle(time);

    coreReq_pipe0_cycle(time);//memRsp的优先级比coreReq高，coreReq pipe1必须在memRsp pipe1之前运行
    memRsp_pipe0_cycle(time);//否则memRsp pipe1的运行会pop memRsp_Q
}

#endif