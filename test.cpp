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

public:
    LSU_2_dcache_coreReq* m_coreReq_ptr;

//private:
    //mshr_miss_req_t* m_miss_req_ptr;

    tag_array m_tag_array;
    mshr m_mshr;
    coreRsp_Q m_coreRsp_Q;
    memReq_Q m_memReq_Q;
};

/*所有可操作的对象包括m_coreRsp_Q, m_mshr.m_miss_req_ptr
*/
void l1_data_cache::cycle(cycle_t time){

    bool mshr_2_coreRsp = false;
    m_mshr.cycle_in(mshr_2_coreRsp, time);

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
                }else if(status == MISS){
                    if (m_mshr.m_miss_req_ptr == nullptr){
                        vec_subentry read_miss_sub = vec_subentry(
                            m_coreReq_ptr->m_reg_idxw,
                            m_coreReq_ptr->m_wid,
                            m_coreReq_ptr->m_mask);
                        mshr_miss_req_t new_miss_req = mshr_miss_req_t(
                            m_coreReq_ptr->m_block_idx,
                            REGULAR_READ_MISS,
                            read_miss_sub);
                        m_mshr.m_miss_req_ptr = &new_miss_req;
                        m_coreReq_ptr = nullptr;
                    }//else 不清空coreReq
                }
            }else{//m_coreReq_ptr->m_type == 1
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
            }
        }
        else if(m_coreReq_ptr->m_opcode==Fence){
            //Fence
        }else {
            assert(m_coreReq_ptr->m_opcode==Amo);
            //AMO
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
    dcache.cycle(102);
    std::cout << dcache.get_tag(0xffff) <<std::endl;

    dcache.m_tag_array.DEBUG_visualize_array(0,10);
    //TODO: How to represent "time"?
    //TODO: How to serialize test event and construct the interface to push test event in
}