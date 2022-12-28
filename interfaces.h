#ifndef INTERFACES_H
#define INTERFACES_H

#include <deque>
#include "parameter.h"

enum LSU_cache_coreReq_opcode {
    Read,
    Write,
    Amo,
    Fence
};

enum TL_UH_A_opcode {
    Get=4,
    PutFullData=0,
    PutPartialData=1,
    ArithmeticData=2,
    LogicalData=3
    //Intent=5
};

class memReq_Q : cache_building_block{
    struct dcache_2_L2_memReq : cache_building_block {
        public:
        dcache_2_L2_memReq(enum TL_UH_A_opcode opcode, u_int32_t param, u_int32_t source_id, u_int64_t block_idx, u_int32_t mask = 0xFFFF){//std::array<u_int32_t,NLINE> *data=NULL,
            a_opcode = opcode;
            a_param = param;
            a_source = source_id;
            a_address = block_idx << LOGB2(NLINE*LINESIZE);
            a_mask = mask;
            if (a_opcode == Get)
                a_data = false;
            else
                a_data = true;
            //a_data = //actually no need to model data in C
        }

        enum TL_UH_A_opcode a_opcode;
        u_int32_t a_param;
        //int a_size;
        u_int32_t a_source; //TODO
        u_int64_t a_address;
        u_int32_t a_mask;
        bool a_data;//only to indicate whether there is a data transaction
        //std::array<u_int32_t,NLINE>* a_data;
    };

public:
    std::deque<dcache_2_L2_memReq> m_Q;
};

struct dcache_2_LSU_coreRsp : cache_building_block {
public:
    dcache_2_LSU_coreRsp(u_int32_t req_id, bool data){//std::array<u_int32_t,NLINE> *data=NULL,std::array<bool,NLANE> mask, 
        m_req_id = req_id;
        //m_mask = mask;
        m_data = data;
    }

    u_int32_t m_req_id;
    //std::array<bool,NLANE> m_mask;
    bool m_data;//only to indicate whether there is a data transaction
    //std::array<u_int32_t,NLINE>* a_data;
};

class coreRsp_Q : cache_building_block {
public:
    std::deque<dcache_2_LSU_coreRsp> m_Q;
};

struct LSU_2_dcache_coreReq : cache_building_block {
public:
    LSU_2_dcache_coreReq(enum LSU_cache_coreReq_opcode opcode, u_int32_t type, 
        u_int32_t wid, u_int32_t req_id, u_int64_t block_idx,vec_nlane_t perLane_addr, 
        std::array<bool,NLANE> mask){//std::array<u_int32_t,NLINE> *data=NULL,
        m_opcode = opcode;
        m_type = type;
        m_wid = wid;
        m_req_id = req_id;//used for coreRsp
        m_block_idx = block_idx;
        m_mask = mask;
        if (opcode == Read | opcode == Fence)
            m_data = false;
        else
            m_data = true;
        //a_data = //actually no need to model data in C
    }

    enum LSU_cache_coreReq_opcode m_opcode;
    u_int32_t m_type;
    u_int32_t m_wid;
    u_int32_t m_req_id;
    u_int64_t m_block_idx;
    std::array<bool,NLANE> m_mask;
    vec_nlane_t m_block_offset;//block_offset
    vec_nlane_t m_word_offset;//block_offset
    bool m_data;//only to indicate whether there is a data transaction
    //std::array<u_int32_t,NLINE>* a_data;
};

struct L2_2_dcache_memRsp : cache_building_block {
    u_int32_t m_req_id;
};

#endif