#ifndef TAG_ARRAY_H
#define TAG_ARRAY_H

#include <iostream>
#include <iomanip>
#include <cmath>
#include <array>
#include <deque>
#include <cassert>
#include <bitset>

#include "utils.h"
#include "parameter.h"
#include "interfaces.h"

enum tag_access_status {
    HIT=1,
    MISS=0
};

class meta_entry_t : cache_building_block{
public:
    meta_entry_t(){
        m_valid = false;
        m_dirty = false;
        m_last_access_time = 0;
        m_last_fill_time = 0;
    }

    u_int32_t tag(){
        return m_tag;
    }

    bool is_valid(){
        return m_valid;
    }

    bool is_dirty(){
        return m_dirty;
    }

    bool is_hit(u_int32_t tag){
        return is_valid() && m_tag == tag;
    }

    void invalidate(){
        m_valid = true;
    }

    void clear_dirty(){
        m_dirty = false;
    }

    void allocate(u_int32_t tag){
        m_tag = tag;
        m_valid = true;
        m_dirty = false;
    }

    void write_dirty(){
        assert(m_valid);
        m_dirty = true;
    }

    void update_access_time(cycle_t new_time){
        m_last_access_time = new_time;
    }
    
    void update_fill_time(cycle_t new_time){
        m_last_fill_time = new_time;
    }

    unsigned int get_access_time(){
        return m_last_access_time;
    }

    unsigned int get_fill_time(){
        return m_last_fill_time;
    }
    
private:
    bool m_valid;
    bool m_dirty;

    u_int64_t m_tag;
    unsigned int m_last_access_time;
    unsigned int m_last_fill_time;

    friend class tag_array;
};

class tag_array : cache_building_block {
public:
    tag_array(){}

    tag_array(memReq_Q& memReq_Q_obj):m_memReq_Q(memReq_Q_obj){}

    enum tag_access_status probe(u_int64_t block_idx, u_int32_t& way_idx);

    void read_hit_update_access_time(u_int32_t way_idx, u_int32_t set_idx,cycle_t time);

    void write_hit_mark_dirty(u_int32_t way_idx, u_int32_t set_idx,cycle_t time);

    //返回值为0时说明当前周期由于memReq_Q阻塞而写入失败
    bool allocate(u_int64_t block_idx,cycle_t time);

    void invalidate_chosen(u_int64_t block_idx);

    void invalidate_all();

    //返回值为0时说明当前周期由于memReq_Q阻塞而写入失败
    bool flush();
    
    /*get the to-be-replaced way in a given set
    return: u_int32_t way_idx
    input u_int32_t set_idx
    */
    u_int32_t replace_choice(u_int32_t set_idx);
    
    //返回值为0时说明当前周期由于memReq_Q阻塞而写入失败
    bool issue_memReq_write(meta_entry_t& line_to_issue, u_int32_t set_idx);

    //for test use only
    void DEBUG_random_initialize(cycle_t time);

    //for test use only
    void DEBUG_visualize_array(u_int32_t set_idx_start, u_int32_t set_idx_end);

    void DEBUG_print_title();

    void DEBUG_print_a_way(u_int32_t set_idx, u_int32_t way_idx);

private:
    std::array<std::array<meta_entry_t, NWAY>,NSET> m_tag;
    memReq_Q m_memReq_Q;//指向真正唯一的memReq_Q
};

#endif