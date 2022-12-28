#ifndef MISS_STATUS_HOLDING_REG_H
#define MISS_STATUS_HOLDING_REG_H

#include <map>

#include "utils.h"
#include "parameter.h"
#include "interfaces.h"

enum entry_target_type{
    REGULAR_READ_MISS,
    LOAD_RESRV,
    STORE_COND,
    AMO
};

class vec_subentry : public cache_building_block{
public:
    vec_subentry(){}

    vec_subentry(enum entry_target_type type, u_int32_t req_id)//,
    //std::array<bool,NLANE> mask,vec_nlane_t block_offset,vec_nlane_t word_offset)
    : m_sub_type(type), m_req_id(req_id){}//, m_mask(mask),
    //m_block_offset(block_offset), m_word_offset(word_offset){}
private:
    //bool m_sub_valid;

    enum entry_target_type m_sub_type; 
    u_int32_t m_req_id;

    //std::array<bool,NLANE> m_mask;
    //vec_nlane_t m_block_offset;
    //vec_nlane_t m_word_offset;

friend class special_target_info;
friend class vec_entry_target_info;
friend class mshr;
};

class vec_entry_target_info : public cache_building_block{
public:
    vec_entry_target_info(){}

    vec_entry_target_info(u_int32_t req_id, vec_subentry sub): m_req_id(req_id){
        allocate_sub(sub);
    }

    bool sub_is_full(){
        return m_sub_en.size() < N_MSHR_SUBENTRY;
    }

    void allocate_sub(const vec_subentry& sub){
        assert(!sub_is_full());
        m_sub_en.push_back(sub);
    }

    void deallocate_sub(){
        assert(!m_sub_en.empty());
        m_sub_en.pop_back();
    }

private:
    //bool m_valid;
    //u_int32_t m_block_addr;

    //便于missRsp时索引，硬件上可能冗余
    u_int32_t m_req_id;
    std::vector<vec_subentry> m_sub_en;

    bool m_have_issued_2_memReq;
    friend class mshr;
};

class special_target_info{
public:
    special_target_info(){}

    special_target_info(vec_subentry sub){
        m_sub_type = sub.m_sub_type;
        m_req_id = sub.m_req_id;
    }

    private:
    enum entry_target_type m_sub_type;
    u_int32_t m_req_id;
    
    bool m_have_issued_2_memReq;
    friend class mshr;
};

class mshr_miss_req_t : public cache_building_block{
public:
    mshr_miss_req_t(u_int32_t block_addr,vec_subentry sub){
        m_block_addr = block_addr;
        m_sub = sub;
    }
private:
    u_int32_t m_block_addr;
    vec_subentry m_sub;

    friend class mshr;
};

class mshr_miss_rsp_t : public cache_building_block{
public:
    mshr_miss_rsp_t(u_int32_t req_id) : m_req_id(req_id){}

    u_int32_t id(){
        return m_req_id;
    }
private:
    u_int32_t m_req_id;
};

class mshr : public cache_building_block{
    typedef u_int32_t block_addr_t;
public:
    mshr(){}

    mshr(coreRsp_Q* coreRsp_Q_ptr) : m_coreRsp_Q_ptr(coreRsp_Q_ptr){
        m_miss_req_ptr = nullptr;
        m_miss_rsp_ptr = nullptr;
    }

    //from special entry
    bool special_arrange_core_rsp(u_int32_t req_id){

        //TODO

        return true;
    }

    //from vec entry
    bool vec_arrange_core_rsp(block_addr_t block_idx){

        //TODO

        return true;
    }

    void cycle_in(bool& mshr_2_coreRsp, cycle_t time){
        if(m_miss_rsp_ptr != nullptr){
            for (auto iter = m_special_entry.begin(); iter != m_special_entry.end(); ++iter) {
                if (iter->first == m_miss_rsp_ptr->id()){
                    //launch a report to coreRsp
                    //mshr_2_coreRsp = arrange_core_rsp();
                    m_miss_rsp_ptr = nullptr;
                }
            }
            for (auto iter = m_vec_entry.begin(); iter != m_vec_entry.end(); ++iter) {
                if (iter->second.m_req_id == m_miss_rsp_ptr->id()){
                    //launch a report to coreRsp
                    //launch a data access write
                    m_miss_rsp_ptr = nullptr;
                }
            }
            assert(m_miss_rsp_ptr==nullptr);//no matching entry in MSHR!
        }else if(m_miss_req_ptr != nullptr){
            if(m_miss_req_ptr->m_sub.m_sub_type == REGULAR_READ_MISS){
                if (is_primary_miss()){
                    assert(m_vec_entry.size() <= N_MSHR_ENTRY);
                    if(m_vec_entry.size() == N_MSHR_ENTRY){
                        return ;
                        std::cout << "primary miss + main entry full at " << time << std::endl;
                    }
                    vec_entry_target_info new_main = vec_entry_target_info(m_miss_req_ptr->m_sub.m_req_id,m_miss_req_ptr->m_sub);
                    m_vec_entry.insert({m_miss_req_ptr->m_block_addr,new_main});//deep copy?
                }else{
                    assert(m_vec_entry.size() > 0);
                    auto& the_main = m_vec_entry[m_miss_req_ptr->m_block_addr];
                    if (the_main.sub_is_full()){
                        return ;
                        std::cout << "secondary miss + sub entry full at " << time << std::endl;
                    }
                    the_main.allocate_sub(m_miss_req_ptr->m_sub);
                }
                //deal with missReq
                m_miss_req_ptr = nullptr;
            }else{//LR SC AMO
                assert(m_special_entry.size() <= N_MSHR_SPECIAL_ENTRY);
                if(m_special_entry.size() == N_MSHR_SPECIAL_ENTRY){
                    return;
                    std::cout << "LR/SC AMO + special entry full at " << time << std::endl;
                }
                special_target_info new_special = special_target_info(m_miss_req_ptr->m_sub);
                m_special_entry.insert({m_miss_req_ptr->m_sub.m_req_id,new_special});
                m_miss_req_ptr = nullptr;
            }
        }
        return;
    }

    void cycle_out(){
        for (auto iter = m_vec_entry.begin(); iter != m_vec_entry.end(); ++iter) {
            if (!iter->second.m_have_issued_2_memReq){

                //TODO

                return;
            }
        }
    }

    bool is_primary_miss(){
        assert(m_miss_req_ptr != NULL);
        for (auto iter = m_vec_entry.begin(); iter != m_vec_entry.end(); ++iter) {
            if (iter->first == m_miss_req_ptr->m_block_addr)
                return true;
        }
        return false;
    }

    mshr_miss_req_t* m_miss_req_ptr;
    mshr_miss_rsp_t* m_miss_rsp_ptr;
    
    private:
    coreRsp_Q* m_coreRsp_Q_ptr;//指向真正唯一的coreRspQ

    std::map<block_addr_t,vec_entry_target_info> m_vec_entry;//TODO
    std::map<uint32_t,special_target_info> m_special_entry;
};

#endif