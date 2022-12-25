#ifndef MISS_STATUS_HOLDING_REG_H
#define MISS_STATUS_HOLDING_REG_H

#include <map>

#include "utils.h"
#include "parameter.h"

enum entry_target_type{
    REGULAR_READ_MISS,
    LOAD_RESRV,
    STORE_COND,
    AMO
};

class subentry : public cache_building_block{
public:
    subentry(){}

    subentry(enum entry_target_type type, u_int32_t req_id,
    std::array<bool,NLANE> mask,vec_nlane_t block_offset,vec_nlane_t word_offset){
        m_sub_type = type;
        m_req_id = req_id;
        m_mask = mask;
        m_block_offset = block_offset;
        m_word_offset = word_offset;
}
private:
    //bool m_sub_valid;
    enum entry_target_type m_sub_type; 
    u_int32_t m_req_id;

    std::array<bool,NLANE> m_mask;
    vec_nlane_t m_block_offset;
    vec_nlane_t m_word_offset;
friend class entry_target_info;
};

class entry_target_info : public cache_building_block{
public:
    entry_target_info(enum entry_target_type type, std::array<bool,NLANE> mask,
        u_int32_t req_id, vec_nlane_t block_offset, vec_nlane_t word_offset){
        allocate_sub(type,mask,req_id,block_offset,word_offset);
    }

    bool sub_is_full(){
        return m_sub_en.size() < N_MSHR_SUBENTRY;
    }

    void allocate_sub(enum entry_target_type type, std::array<bool,NLANE> mask,
        u_int32_t req_id, vec_nlane_t block_offset, vec_nlane_t word_offset){
        assert(!sub_is_full());
        subentry sub = subentry(type, req_id, mask, block_offset, word_offset);
        m_sub_en.push_back(sub);
    }

    void deallocate_sub(){
        assert(!m_sub_en.empty());
        m_sub_en.pop_back();
    }

private:
    //bool m_valid;
    //u_int32_t m_block_addr;
    std::vector<subentry> m_sub_en;
};

class mshr_miss_req_t : public cache_building_block{
    mshr_miss_req_t(u_int32_t block_addr, u_int32_t req_id,
        enum entry_target_type type, subentry* sub_ptr){
        m_block_addr = block_addr;
        m_sub_ptr = sub_ptr;
    }
    private:
    u_int32_t m_block_addr;
    subentry* m_sub_ptr;

    friend class mshr;
};

class mshr_miss_rsp_t : public cache_building_block{
    private:
    u_int32_t m_req_id;
};

class mshr : public cache_building_block{
    typedef u_int32_t block_addr_t;
    mshr(){
        m_miss_req_ptr = NULL;
        m_miss_rsp_ptr = NULL;
    }
    void cycle(){
        if(m_miss_rsp_ptr != NULL){
            //deal with miss rsp//TODO
            m_miss_rsp_ptr = NULL;
        }else if(m_miss_req_ptr != NULL){
            if (is_primary_miss()){
                assert(m_entry.size() <= N_MSHR_ENTRY);
                if(m_entry.size() == N_MSHR_ENTRY)
                    return ;//primary miss + main entry full
                //entry_target_info new_main(m_miss_req_ptr->m_type,//TODO
                //m_miss_req_ptr->m_mask,m_miss_req_ptr->m_req_id,
                //m_miss_req_ptr->m_block_offset,m_miss_req_ptr->m_word_offset);
                //m_entry.insert({m_miss_req_ptr->m_block_addr,m_miss_req_ptr->})
            }else{
                if (m_entry[m_miss_req_ptr->m_block_addr].sub_is_full())
                    return ;//secondary miss + sub entry full
            }
            //deal with missReq
            m_miss_req_ptr = NULL;
        }
        return;
    }

    bool is_primary_miss(){
        assert(m_miss_req_ptr != NULL);
        for (auto iter = m_entry.begin(); iter != m_entry.end(); ++iter) {
            if (iter->first == m_miss_req_ptr->m_block_addr)
                return true;
        }
        return false;
    }

    private:
    mshr_miss_req_t* m_miss_req_ptr;
    mshr_miss_rsp_t* m_miss_rsp_ptr;
    std::map<block_addr_t,entry_target_info> m_entry;//TODO
};

#endif