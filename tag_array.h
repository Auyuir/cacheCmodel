#include <iostream>
#include <iomanip>
#include <cmath>
#include <array>
#include <deque>
#include <cassert>
#include <bitset>

#include "utils.h"

enum tag_access_status {
    HIT=1,
    MISS=0
};

typedef unsigned int cycle_t;

class cache_building_block{
public:
    u_int64_t get_tag(u_int64_t block_idx){//TODO
        return 0;
    }
    u_int64_t get_set_idx(u_int64_t block_idx){//TODO
        return 0;
    }

public:
  //                                       |   blockOffset  |
  //                                       |             wordOffset
  // |32      tag       22|21   setIdx   11|10 9|8         2|1 0|
    constexpr static u_int32_t WORDSIZE = 4;//in bytes
    constexpr static u_int32_t ADDR_LENGTH = 32;

    constexpr static u_int32_t NLANE = 32;

    constexpr static u_int32_t NSET = 32;//For 4K B per way
    constexpr static u_int32_t NWAY = 2;

    constexpr static u_int32_t NLINE = NSET * NWAY;
    constexpr static u_int32_t LINEWORDS = NLANE;//TODO: decouple this param with NLANE
    constexpr static u_int32_t LINESIZE = LINEWORDS * WORDSIZE;//in bytes
    //TODO assert(check LINESIZE to be the power of 2)
    constexpr static u_int32_t CACHESIZE = NLINE *LINESIZE;

    const static u_int32_t DATA_SRAM_LATENCY = 0;//in cycle
};

class meta_entry_t : cache_building_block{
public:
    meta_entry_t(){
        m_valid = false;
        m_dirty = false;
        m_last_access_time = 0;
        m_last_fill_time = 0;
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
    /*tag_array(memReq_Q* memReq_Q_ptr ){
        //m_data_array_ptr = data_array_ptr;
        //m_memReq_Q_ptr = memReq_Q_ptr;
    }*/

    enum tag_access_status probe(u_int64_t block_idx, u_int32_t& way_idx);

    void read_hit_update_access_time(u_int32_t way_idx, u_int32_t set_idx,cycle_t time);

    void write_hit_mark_dirty(u_int32_t way_idx, u_int32_t set_idx,cycle_t time);

    void allocate(u_int64_t block_idx,cycle_t time);

    void invalidate_chosen(u_int64_t block_idx);

    void invalidate_all();

    void flush();
    
    /*get the to-be-replaced way in a given set
    return: u_int32_t way_idx
    input u_int32_t set_idx
    */
    u_int32_t replace_choice(u_int32_t set_idx);
    
    void issue_memReq_write(meta_entry_t& line_to_issue);

    //for test use only
    void DEBUG_random_initialize(cycle_t time);

    //for test use only
    void DEBUG_visualize_array(u_int32_t set_idx_start, u_int32_t set_idx_end);

    void DEBUG_print_title();

    void DEBUG_print_a_way(u_int32_t set_idx, u_int32_t way_idx);

private:
    std::array<std::array<meta_entry_t, NWAY>,NSET> m_tag;
    //data_array* m_data_array_ptr;
    //memReq_Q* m_memReq_Q_ptr;
};