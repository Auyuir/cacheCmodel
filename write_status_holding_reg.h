#ifndef WRITE_STATUS_HOLDING_REG_H
#define WRITE_STATUS_HOLDING_REG_H

#include "utils.h"
#include "parameter.h"
//#include "interfaces.h"

class wshr : public cache_building_block{
    public:
    wshr(){}

    bool is_full(wshr_idx_t& idx){
        for(int i=0;i<N_WSHR_ENTRY;++i){
            if(m_valid[i]  == false){
                idx = i;
                return false;
            }
        }
        return true;
    }

    bool has_conflict(block_addr_t block_idx){
        for(int i=0;i<N_WSHR_ENTRY;++i){
            if(m_valid[i] && (m_entry[i] == block_idx)){
                return true;
            }
        }
        return false;
    }

    void enqueue(block_addr_t block_idx, wshr_idx_t idx){
        assert(!has_conflict(block_idx));
        assert(!m_valid[idx]);
        m_valid[idx] = true;
        m_entry[idx] = block_idx;
    }

    private:
    std::array<bool,N_WSHR_ENTRY> m_valid;
    std::array<block_addr_t,N_WSHR_ENTRY> m_entry;
};

#endif