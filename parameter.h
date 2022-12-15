#ifndef PARAMETERS_H
#define PARAMETERS_H

#include <iostream>

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

#endif