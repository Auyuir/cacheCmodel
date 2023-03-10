#ifndef INTERFACES_H
#define INTERFACES_H

#include <deque>
#include "parameter.h"

enum LSU_cache_coreReq_opcode {
    Read,
    Write,
    Amo,
    InvOrFlu
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

enum TL_UH_D_opcode {
    AccessAck=0,
    AccessAckData=1,
};

class pipe_reg_base {
    public:
    pipe_reg_base(){
        m_valid = false;
    }

    bool is_valid(){
        return m_valid;
    }

    void invalidate(){
        m_valid = false;
    }

    void set_valid(){
        m_valid = true;
    }

    private:
    bool m_valid;
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
    bool is_full(){
        assert(m_Q.size() <= MEM_REQ_Q_DEPTH);
        return m_Q.size() == MEM_REQ_Q_DEPTH;
    }

    bool is_empty(){
        return m_Q.size() == 0;
    }

    std::deque<dcache_2_L2_memReq> m_Q;
};

struct L2_2_dcache_memRsp : cache_building_block {
    L2_2_dcache_memRsp(){}
    L2_2_dcache_memRsp(u_int32_t req_id):m_req_id(req_id){}
    u_int32_t m_req_id;
};

class memRsp_Q : cache_building_block{
public:
    bool is_full(){
        assert(m_Q.size() <= MEM_RSP_Q_DEPTH);
        return m_Q.size() == MEM_RSP_Q_DEPTH;
    }

    std::deque<L2_2_dcache_memRsp> m_Q;
};

struct dcache_2_LSU_coreRsp : cache_building_block {
public:
    dcache_2_LSU_coreRsp(){}

    dcache_2_LSU_coreRsp(u_int32_t reg_idxw, bool data, 
        u_int32_t wid, std::array<bool,NLANE> mask):m_wid(wid),
        m_reg_idxw(reg_idxw),m_mask(mask),m_data(data){
        bool m_wxd = true;//IsScalar?
        for (int i = 1;i<NLANE;++i){
            if (m_mask[i]==true){
                m_wxd = false;
                break;
            }
        }
    }

    u_int32_t m_wid;
    u_int32_t m_reg_idxw;
    std::array<bool,NLANE> m_mask;
    bool m_wxd;//indicate whether its a scalar instruction
    bool m_data;//only to indicate whether there is a data transaction
    //std::array<u_int32_t,NLINE>* a_data;
};

class coreRsp_Q : cache_building_block {
public:
    bool is_full(){
        assert(m_Q.size() <= CORE_RSP_Q_DEPTH);
        return m_Q.size() == CORE_RSP_Q_DEPTH;
    }

    bool is_empty(){
        return m_Q.size() == 0;
    }

    void DEBUG_print(cycle_t time){
        auto& tar = m_Q.front();
        std::cout << std::setw(5) << time << " | coreRsp " << DEBUG_print_number;
        std::cout << " : reg_idx" << tar.m_reg_idxw << " ";
        std::cout << "wid " << tar.m_wid;
        if (tar.m_wxd)
            std::cout << ", is scalar ";
        else
            std::cout << ", is vector ";
        
        if(!tar.m_data)
            std::cout << ", no data " << std::endl;
        else
            std::cout << ", with data " << std::endl;
        ++DEBUG_print_number;
    }

    std::deque<dcache_2_LSU_coreRsp> m_Q;
    
private:
    int DEBUG_print_number=0;
};

struct LSU_2_dcache_coreReq : cache_building_block {
public:
    LSU_2_dcache_coreReq(){}

    LSU_2_dcache_coreReq(enum LSU_cache_coreReq_opcode opcode, u_int32_t type, 
        u_int32_t wid, u_int32_t req_id, u_int64_t block_idx,vec_nlane_t perLane_addr, 
        std::array<bool,NLANE> mask, enum LSU_cache_coreReq_type_amo amo_type=notamo):
        m_opcode(opcode), m_type(type), m_wid(wid), m_reg_idxw(req_id), m_block_idx(block_idx), 
        m_mask(mask), m_amo_type(amo_type){
        if (opcode == Read | opcode == InvOrFlu)
            m_data = false;
        else
            m_data = true;
        //a_data = //actually no need to model data in C
    }

    enum LSU_cache_coreReq_opcode m_opcode;
    u_int32_t m_type;
    enum LSU_cache_coreReq_type_amo m_amo_type;//??????????????????????????????type??????????????????
    u_int32_t m_wid;//used for coreRsp
    u_int32_t m_reg_idxw;//used for coreRsp
    u_int64_t m_block_idx;
    std::array<bool,NLANE> m_mask;
    vec_nlane_t m_block_offset;//block_offset
    vec_nlane_t m_word_offset;//block_offset
    bool m_data;//only to indicate whether there is a data transaction
    //std::array<u_int32_t,NLINE>* a_data;
};

class coreReq_pipe_reg : public LSU_2_dcache_coreReq, public pipe_reg_base{
    public:
    coreReq_pipe_reg(){}

    void update_with(LSU_2_dcache_coreReq coreReq){
        m_opcode=coreReq.m_opcode;
        m_type=coreReq.m_type;
        m_wid=coreReq.m_wid;
        m_reg_idxw=coreReq.m_reg_idxw;
        m_block_idx=coreReq.m_block_idx;
        m_mask=coreReq.m_mask;
        m_amo_type=coreReq.m_amo_type;
        if (coreReq.m_opcode == Read | coreReq.m_opcode == InvOrFlu)
            m_data = false;
        else
            m_data = true;
        set_valid();
    }

    /*coreReq_pipe_reg(enum LSU_cache_coreReq_opcode opcode, u_int32_t type, 
        u_int32_t wid, u_int32_t req_id, u_int64_t block_idx,vec_nlane_t perLane_addr, 
        std::array<bool,NLANE> mask, enum LSU_cache_coreReq_type_amo amo_type=notamo):
        LSU_2_dcache_coreReq(opcode,type,wid,req_id,block_idx,perLane_addr,mask,amo_type){
        set_valid();
    }*/
};

class coreRsp_pipe_reg : public dcache_2_LSU_coreRsp, public pipe_reg_base{
    public:
    coreRsp_pipe_reg(){}

    void update_with(dcache_2_LSU_coreRsp coreRsp){
        m_wid = coreRsp.m_wid;
        m_reg_idxw = coreRsp.m_reg_idxw;
        m_mask = coreRsp.m_mask;
        m_wxd = coreRsp.m_wxd;
        m_data = coreRsp.m_data;
        set_valid();
    }
    /* coreRsp_pipe_reg(u_int32_t reg_idxw, bool data, u_int32_t wid, 
        std::array<bool,NLANE> mask):dcache_2_LSU_coreRsp(reg_idxw,data,wid,mask){
        set_valid();
    } */
};

#endif