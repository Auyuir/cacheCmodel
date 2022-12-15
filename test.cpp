#include "parameter.h"
#include "tag_array.h"

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
    dcache_2_LSU_coreRsp(u_int32_t req_id, std::array<bool,NLANE> mask, bool data){//std::array<u_int32_t,NLINE> *data=NULL,
        m_req_id = req_id;
        m_mask = mask;
        m_data = data;
    }

    u_int32_t m_req_id;
    std::array<bool,NLANE> m_mask;
    bool m_data;//only to indicate whether there is a data transaction
    //std::array<u_int32_t,NLINE>* a_data;
};

class coreRsp_Q : cache_building_block {
public:
    std::deque<dcache_2_LSU_coreRsp> m_Q;
};

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

struct LSU_2_dcache_coreReq : cache_building_block {
public:
    LSU_2_dcache_coreReq(enum LSU_cache_coreReq_opcode opcode, u_int32_t type, 
        u_int32_t wid, u_int32_t req_id, u_int64_t block_idx,std::array<u_int32_t,NLANE> perLane_addr, 
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
    std::array<u_int32_t,NLANE> m_perLane_addr;//block_offset
    bool m_data;//only to indicate whether there is a data transaction
    //std::array<u_int32_t,NLINE>* a_data;
};

class l1_data_cache : public cache_building_block{
public:
    l1_data_cache(){
        m_coreReq_ptr=NULL;
    }
    
    void cycle(cycle_t time);

public:
    LSU_2_dcache_coreReq* m_coreReq_ptr;
//private:
    tag_array m_tag_array;
    coreRsp_Q m_coreRsp_Q;
};

void l1_data_cache::cycle(cycle_t time){
    if (m_coreReq_ptr == NULL)
        return;
    else{
        if (m_coreReq_ptr->m_opcode==Read || m_coreReq_ptr->m_opcode==Write){
            assert(m_coreReq_ptr->m_type == 0 || m_coreReq_ptr->m_type == 1);
            if(m_coreReq_ptr->m_type == 0){//TODO
                //Regular READ or WRITE
                u_int32_t way_idx=0;
                enum tag_access_status status = m_tag_array.probe(m_coreReq_ptr->m_block_idx,way_idx);
                if (status == HIT){
                    m_tag_array.read_hit_update_access_time(m_coreReq_ptr->m_block_idx,way_idx,time);
                    //TODO  data
                    dcache_2_LSU_coreRsp read_hit_coreRsp(m_coreReq_ptr->m_req_id,m_coreReq_ptr->m_mask,true);
                    m_coreRsp_Q.m_Q.push_back(read_hit_coreRsp);
                    m_coreReq_ptr = NULL;
                }else if(status == MISS){
                    //TODO  mshr
                }
            }else if(m_coreReq_ptr->m_type == 1){//TODO
                //LR / SC
            }
        }else if(m_coreReq_ptr->m_opcode==Fence){
            //Fence
        }else {
            assert(m_coreReq_ptr->m_opcode==Amo);
        }
    }
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
    std::cout << dcache.get_tag(0xffff) <<std::endl;

    dcache.m_tag_array.DEBUG_visualize_array(0,10);
    //TODO: How to represent "time"?
    //TODO: How to serialize test event and construct the interface to push test event in
}