#include "tag_array.h"

enum tag_access_status tag_array::probe(u_int64_t block_idx, u_int32_t& way_idx){
        u_int32_t set_idx = get_set_idx(block_idx);
        u_int64_t tag = get_tag(block_idx);
        for (int i=0;i<NWAY;++i){
            if (m_tag[set_idx][i].is_hit(tag)){
                way_idx = i;
                return HIT;
            }
        }
        return MISS;
    }

    void tag_array::read_hit_update_access_time(u_int32_t way_idx, u_int32_t set_idx,cycle_t time){
        auto& the_one = m_tag[set_idx][way_idx];
        assert(the_one.is_valid());
        the_one.update_access_time(time);
    }

    void tag_array::write_hit_mark_dirty(u_int32_t way_idx, u_int32_t set_idx,cycle_t time){
        auto& the_one = m_tag[set_idx][way_idx];
        assert(the_one.is_valid());
        the_one.write_dirty();
        the_one.update_access_time(time);
    }

    void tag_array::allocate(u_int64_t block_idx,cycle_t time){
        u_int32_t set_idx = get_set_idx(block_idx);
        u_int32_t way_idx = replace_choice(set_idx);
        auto& the_one = m_tag[set_idx][way_idx];
        if (the_one.is_dirty())
            issue_memReq_write(the_one);
        the_one.allocate(get_tag(block_idx));
        the_one.update_fill_time(time);
        the_one.update_access_time(time);
    }

    void tag_array::invalidate_chosen(u_int64_t block_idx){
        u_int32_t set_idx = get_set_idx(block_idx);
        u_int64_t tag = get_tag(block_idx);
        for (int i=0;i<NWAY;++i){
            if (m_tag[set_idx][i].is_hit(tag))
                m_tag[set_idx][i].invalidate();
            break;
        }
    }

    void tag_array::invalidate_all(){
        //this function must be called after tag_array::flush()
        for (int i=0;i<NSET;++i){
            for (int j=0;j<NWAY;++j){
                auto& the_one = m_tag[i][j];
                //assert())dirty?
                the_one.invalidate();
            }
        }
    }

    void tag_array::flush(){
        //write back all the dirty lines
        for (int i=0;i<NSET;++i){
            for (int j=0;j<NWAY;++j){
                auto& the_one = m_tag[i][j];
                if (the_one.is_valid() && the_one.is_dirty()){
                    issue_memReq_write(the_one);
                    the_one.clear_dirty();
                }
            }
        }
    }
    
    /*get the to-be-replaced way in a given set
    return: u_int32_t way_idx
    input u_int32_t set_idx
    */
    u_int32_t tag_array::replace_choice(u_int32_t set_idx){
        //LRU
        u_int32_t way_idx;
        unsigned int least_recently_time=(unsigned)-1;
        u_int32_t least_recently_idx=0;
        for (int i=0;i<NWAY;++i){
            auto& the_one = m_tag[set_idx][i];
            if(!the_one.is_valid())
                return i;
            if(least_recently_time < the_one.get_access_time()){
                least_recently_time = the_one.get_access_time();
                least_recently_idx = i;
            }
        }
        return least_recently_idx;
    }
    
    void tag_array::issue_memReq_write(meta_entry_t& line_to_issue){//TODO
        //push the write back request to memReqQ(store q)
    }

    //for test use only
    void tag_array::DEBUG_random_initialize(cycle_t time){
        u_int32_t BLOCK_OFFSET_BITS = LOGB2(WORDSIZE);
        u_int32_t TAG_BITS = ADDR_LENGTH - BLOCK_OFFSET_BITS - LOGB2(WORDSIZE);
        for (int i=0;i<NSET;++i){
            for (int j=0;j<NWAY;++j){
                auto& the_one = m_tag[i][j];
                if (randomBool()){//randomly allocate,
                    the_one.allocate(random<u_int32_t>(0,POW2(TAG_BITS/4)));
                    cycle_t rand_fill_time = random<cycle_t>(0,time);
                    the_one.update_fill_time(rand_fill_time);
                    the_one.update_access_time(rand_fill_time);
                    if (randomBool()){//randomly re-access,
                        the_one.update_access_time(random<cycle_t>(rand_fill_time,time));
                        if (randomBool()){//some of them are write
                            the_one.write_dirty();
                        }
                    }
                }else
                    break;
            }
        }
    }

    //for test use only
    void tag_array::DEBUG_visualize_array(u_int32_t set_idx_start, u_int32_t set_idx_end){
        DEBUG_print_title();
        for(int i=set_idx_start;i<set_idx_start+set_idx_end;++i){
            std::cout << std::setw(7) << i << " |";
            for (int j=0;j<NWAY;++j){
                DEBUG_print_a_way(i,j);
            }
            std::cout << std::endl;
        }
    }

    void tag_array::DEBUG_print_title(){
        std::cout << "set_idx ";
        for(int i=0;i<NWAY;++i){
            std::cout << "| way "<< i <<"               ";
        }
        std::cout <<"|"<< std::endl << std::setw(8) <<" ";
        for(int i=0;i<NWAY;++i){
            std::cout << "| v d --tag--- lat lft";
        }
        std::cout << std::endl;
    }

    void tag_array::DEBUG_print_a_way(u_int32_t set_idx, u_int32_t way_idx){
        auto& the_one = m_tag[set_idx][way_idx];
        std::bitset<ADDR_LENGTH/4> tag(the_one.m_tag);
        std::cout << " " << the_one.is_valid() << " " << the_one.is_dirty() << " " << tag;
        std::cout << " " << std::setw(3) << the_one.get_access_time();
        std::cout << " " << std::setw(3) << the_one.get_fill_time() << "|";
    }