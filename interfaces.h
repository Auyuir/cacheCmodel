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

enum LSU_cache_coreReq_type_amo {
    notamo = 0xFF,
    amoadd = 0x00,
    amoxor = 0x01,
    amoand = 0x03,
    amoor = 0x02,
    amomin = 0x04,
    amomax = 0x05,
    amominu = 0x06,
    amomaxu = 0x07,
    amoswap = 0x10
};

enum TL_UH_A_PARAM_AMOARITH {
    MIN = 0x00,
    MAX = 0x01,
    MINU = 0x02,
    MAXU = 0x03,
    ADD = 0x04
};

enum TL_UH_A_PARAM_AMOLOGIC{
    XOR = 0x00,
    OR = 0x01,
    AND = 0x02,
    SWAP = 0x03
};

enum TL_UH_A_opcode {
    Get=4,
    PutFullData=0,
    PutPartialData=1,
    ArithmeticData=2,
    LogicalData=3
    //Intent=5
};

struct dcache_2_L2_memReq : cache_building_block {
    public:
    dcache_2_L2_memReq(){}

    dcache_2_L2_memReq(enum TL_UH_A_opcode opcode, 
        u_int32_t param, u_int32_t source_id, u_int64_t block_idx) 
        : a_opcode(opcode), a_param(param),
        a_source(source_id){
        a_address = block_idx << LOGB2(NLINE*LINESIZE);
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
    //u_int32_t a_mask;
    bool a_data;//only to indicate whether there is a data transaction
    //std::array<u_int32_t,NLINE>* a_data;
};

class memReq_Q : cache_building_block{
public:
    std::deque<dcache_2_L2_memReq> m_Q;
};

struct dcache_2_LSU_coreRsp : cache_building_block {
public:
    dcache_2_LSU_coreRsp(u_int32_t reg_idxw, bool data){//std::array<u_int32_t,NLINE> *data=NULL,std::array<bool,NLANE> mask, 
        m_reg_idxw = reg_idxw;
        //m_mask = mask;
        m_data = data;
    }

    u_int32_t m_reg_idxw;
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
        m_wid = wid;//used for coreRsp
        m_reg_idxw = req_id;//used for coreRsp
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
    u_int32_t m_reg_idxw;
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