/*
  Ghost File System, or simply GhostFS
  Copyright (C) 2016 Raphael S. Carvalho

  This program can be distributed under the terms of the GNU GPL.
  See the file COPYING.
*/

#ifndef CACHE_H
#define CACHE_H

#include <mutex>

#include "block_info.h"

struct cache {
private:
    bi::list<block, bi::member_hook<block, bi::list_member_hook<>,
        &block::_lru_link>> _lru;
    size_t _blocks_used = 0;
    size_t _blocks_available;
    size_t _block_size;

    size_t blocks_used();
public:
    std::mutex _mtx;

    cache(size_t blocks, size_t block_size);

    ~cache();

    // Return a block which isn't inserted to lru yet, i.e. the block is locked.
    block* allocate_block(block_info* info);

    void lock_block(block* blk);

    void unlock_block(block* blk);

    size_t block_size();

    friend struct ghost_fs;
};

#endif // CACHE_H
