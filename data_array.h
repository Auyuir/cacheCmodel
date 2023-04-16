#ifndef DATA_ARRAY_H
#define DATA_ARRAY_H

#include "utils.h"
#include "parameter.h"
#include "interfaces.h"

class tag_array : cache_building_block {

    //return: 数据
    cache_line_t read(u_int32_t set_idx,u_int32_t way_idx){
        return data[set_idx][way_idx];
    }

    void fill_write(u_int32_t set_idx,u_int32_t way_idx,cache_line_t& fill_line){
        data[set_idx][way_idx] = fill_line;
    }

    void write_hit(u_int32_t set_idx,u_int32_t way_idx, 
        vec_nlane_t hit_data, vec_nlane_t block_offset, std::array<bool,NLANE> lane_mask){
        auto& selected_line = data[set_idx][way_idx];
        for(int i = 1;i<NLANE;++i){
            if(lane_mask[i]==true){
                selected_line[block_offset[i]] = hit_data[i];
            }
        }
    }

private:
    std::array<std::array<cache_line_t, NWAY>,NSET> data;
};
#endif