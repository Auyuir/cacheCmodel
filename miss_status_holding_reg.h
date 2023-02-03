#ifndef MISS_STATUS_HOLDING_REG_H
#define MISS_STATUS_HOLDING_REG_H

#include <map>
#include <deque>

#include "utils.h"
#include "parameter.h"
#include "interfaces.h"

enum entry_target_type{
    REGULAR_READ_MISS,
    LOAD_RESRV,
    STORE_COND,
    AMO
};

enum vec_mshr_status{
    PRIMARY_AVAIL,
    PRIMARY_FULL,
    SECONDARY_AVAIL,
    SECONDARY_FULL
};

enum spe_mshr_status{
    AVAIL,
    FULL
};

class vec_subentry : public cache_building_block{
public:
    vec_subentry(){}

    vec_subentry(u_int32_t req_id,u_int32_t wid,std::array<bool,NLANE> mask)//,
    //vec_nlane_t block_offset,vec_nlane_t word_offset)
    :m_req_id(req_id), m_wid(wid), m_mask(mask){}//,
    //m_block_offset(block_offset), m_word_offset(word_offset){}
private:
    u_int32_t m_req_id;
    u_int32_t m_wid;

    std::array<bool,NLANE> m_mask;
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
    std::deque<vec_subentry> m_sub_en;

    friend class mshr;
};

class special_target_info{
public:
    //MSHR中不记录amo类型，因为MSHR中记录的信息仅用于coreRsp，不再用于missReq
    //missReq相关职能移入memReq_Q
    special_target_info(){}

    special_target_info(enum entry_target_type type, u_int32_t wid):
        m_type(type),m_wid(wid){}

private:
    enum entry_target_type m_type;
    //u_int32_t m_req_id;作为speMSHR entry的索引了
    u_int32_t m_wid;
    //enum LSU_cache_coreReq_type_amo m_amo_type;
    //block_addr_t m_block_idx;
    
    friend class mshr;
};

//本类的成员变量和missRsp_process的入参相同
class mshr_miss_rsp : public cache_building_block{
public:
    mshr_miss_rsp(enum entry_target_type type, u_int32_t req_id, 
        block_addr_t block_idx):m_type(type), m_req_id(req_id), m_block_idx(block_idx){}

private:
    enum entry_target_type m_type;
    u_int32_t m_req_id;
    block_addr_t m_block_idx;

friend class mshr;
};

class mshr : public cache_building_block{
public:
    mshr(){}

    mshr(dcache_2_LSU_coreRsp*& coreRsp_pipe_reg_obj, memReq_Q& memReq_Q_obj, tag_array& tag_obj)
        :m_coreRsp_pipe2_reg_ptr(coreRsp_pipe_reg_obj), m_memReq_Q(memReq_Q_obj), m_tag_array(tag_obj){}

    //from special entry
    void special_arrange_core_rsp(u_int32_t req_id){
        assert(m_coreRsp_pipe2_reg_ptr == nullptr);
        auto& the_entry = m_special_entry[req_id];
        std::array<bool,NLANE> mask_of_scalar = {true };
        dcache_2_LSU_coreRsp new_rsp = dcache_2_LSU_coreRsp(
            req_id,true,the_entry.m_wid,mask_of_scalar);
        m_coreRsp_pipe2_reg_ptr = &new_rsp;
        //AMO,LR,SC都不引起data access写入。
        //AMO和LR向coreRsp.data写回数据
        //SC成功向coreRsp.data写0，失败写1
        //coreRsp.data的具体内容在本模型中不会体现
        m_special_entry.erase(req_id);
        return;
    }

    //每次调用处理一个subentry。当前main entry清空时，返回true。
    bool vec_arrange_core_rsp(block_addr_t block_idx){
        assert(m_coreRsp_pipe2_reg_ptr == nullptr);
        auto& current_main = m_vec_entry[block_idx].m_sub_en;
        assert(!current_main.empty());
        auto& current_sub = current_main.front();
        dcache_2_LSU_coreRsp new_rsp = dcache_2_LSU_coreRsp(
            current_sub.m_req_id,true,current_sub.m_wid,current_sub.m_mask);
        m_coreRsp_pipe2_reg_ptr = &new_rsp;
        current_main.pop_front();

        if (current_main.empty()){
            m_vec_entry.erase(block_idx);
            return true;
        }
        
        return false;
    }

    enum vec_mshr_status probe_vec(block_addr_t block_idx){
        if(is_primary_miss(block_idx)){
            assert(m_vec_entry.size() <= N_MSHR_ENTRY);
            if(m_vec_entry.size() == N_MSHR_ENTRY){
                return PRIMARY_FULL;
                //std::cout << "primary miss + main entry full at " << time << std::endl;//TODO: 分级debug info机制
            }else{
                return PRIMARY_AVAIL;
            }
        }else{
            assert(m_vec_entry.size() > 0);
            auto& the_main = m_vec_entry[block_idx];
            if (the_main.sub_is_full()){
                return SECONDARY_FULL;
                //std::cout << "secondary miss + sub entry full at " << time << std::endl;
            }else{
                return SECONDARY_AVAIL;
            }
        }
    }

    enum spe_mshr_status probe_spe(){
        assert(m_special_entry.size() <= N_MSHR_SPECIAL_ENTRY);
        if(m_special_entry.size() == N_MSHR_SPECIAL_ENTRY){
            return FULL;
            //std::cout << "LR/SC AMO + special entry full at " << time << std::endl;
        }else{
            return AVAIL;
        }
    }

    void allocate_vec_main(block_addr_t block_idx, vec_subentry& vec_sub){
        vec_entry_target_info new_main = vec_entry_target_info(vec_sub.m_req_id,vec_sub);
        m_vec_entry.insert({block_idx,new_main});//deep copy?
    }

    void allocate_vec_sub(block_addr_t block_idx, vec_subentry& vec_sub){
        auto& the_main = m_vec_entry[block_idx];
        the_main.allocate_sub(vec_sub);
    }

    void allocate_special(enum entry_target_type type, 
        u_int32_t req_id, u_int32_t wid){
        special_target_info new_special;
        new_special = special_target_info(type, wid);
        m_special_entry.insert({req_id, new_special});
    }

    bool is_primary_miss(block_addr_t block_idx){
        //需要转换MSHR存储类型时（reg/SRAM）可以从这里着手考虑
        for (auto iter = m_vec_entry.begin(); iter != m_vec_entry.end(); ++iter) {
            if (iter->first == block_idx)
                return true;
        }
        return false;
    }

    enum entry_target_type detect_missRsp_type(block_addr_t& block_idx, u_int32_t req_id){
        auto spe_iter = m_special_entry.find(req_id);
        if(spe_iter != m_special_entry.end()){//LR/SC/AMO
            return spe_iter->second.m_type;
        }else{//confirm regular miss
            for (auto vec_iter = m_vec_entry.begin(); vec_iter != m_vec_entry.end(); ++vec_iter) {
                if(req_id == vec_iter->second.m_req_id){
                    block_idx = vec_iter->first;
                    return REGULAR_READ_MISS;
                }
            }
            std::cout << "missRsp no entry in mshr" << std::endl;
            assert(false);
        }
    }

    //返回值代表下个周期是否可以清除mshr寄存器，让新missRsp进入
    bool missRsp_process(const mshr_miss_rsp& miss_rsp, cycle_t time){
        auto& type = miss_rsp.m_type;
        auto& block_idx = miss_rsp.m_block_idx;
        auto& req_id = miss_rsp.m_req_id;
        if (type == REGULAR_READ_MISS){
            bool allocate_success = true;
            if(!tag_req_current_missRsp_has_sent){
                allocate_success = m_tag_array.allocate(block_idx, time);
                //本建模不体现，硬件在这里需要启动data SRAM的更新
            }
            if(m_vec_entry.size() > 0){
                if(m_coreRsp_pipe2_reg_ptr == nullptr){
                    bool main_finish = vec_arrange_core_rsp(block_idx);
                    if(main_finish){
                        tag_req_current_missRsp_has_sent = !allocate_success;
                        return allocate_success;
                    }
                }
            }else{
                //本建模中vec_entry不会为0，因为没有建模data SRAM的多周期写入行为
                //所以这条路径不会被触发
                tag_req_current_missRsp_has_sent = !allocate_success;
                return allocate_success;
            }
        }else{//AMO/LR/SC
            if(m_coreRsp_pipe2_reg_ptr == nullptr){
                special_arrange_core_rsp(req_id);
                return true;
            }
        }
        return false;
    }
    
    private:
    memReq_Q m_memReq_Q;
    //coreRsp_Q m_coreRsp_Q;
    dcache_2_LSU_coreRsp* m_coreRsp_pipe2_reg_ptr;
    tag_array m_tag_array;//指向真正唯一的tag_array

    std::map<block_addr_t,vec_entry_target_info> m_vec_entry;
    std::map<uint32_t,special_target_info> m_special_entry;

    bool tag_req_current_missRsp_has_sent;
};

#endif