/*
  Ghost File System, or simply GhostFS
  Copyright (C) 2016 Raphael S. Carvalho

  This program can be distributed under the terms of the GNU GPL.
  See the file COPYING.
*/

#include "cache.h"

size_t cache::blocks_used() {
    return _blocks_used;
}

cache::cache(size_t blocks, size_t block_size)
    : _blocks_available(blocks)
    , _block_size(block_size) {}

cache::~cache() {
    _lru.clear();
}

block *cache::allocate_block(block_info *info) {
    assert(_lru.size() <= _blocks_available);
    assert(blocks_used() <= _blocks_available);
    assert(info->_present == false);
    assert(info->_blk == nullptr);

    block *blk = nullptr;

    if (blocks_used() < _blocks_available) {
        char *data = new char[_block_size];

        blk = new block(info, data);
        // Make block_info of the caller store allocated block.
        info->set_block(blk);

        //_lru.push_front(reinterpret_cast<block&>(*blk));
        _blocks_used++;
    } else {
        // XXX: possible race condition with eviction.
        // If we evict a block which is about to be used but wasn't locked yet.

        assert(!_lru.empty());

        block& victim = reinterpret_cast<block&>(_lru.back());
        _lru.erase(_lru.iterator_to(victim)); // lock evicted block.
        assert(victim._info);

        // Reset block_info of the evicted block.
        victim._info->reset();
        // Make block_info of the caller store evicted block.
        info->set_block(&victim);
        // Make evicted block store block_info of the caller.
        victim._info = info;
        blk = &victim;
    }

    return blk;
}

void cache::lock_block(block *blk) {
    auto& blk_ref = reinterpret_cast<block&>(*blk);
    _lru.erase(_lru.iterator_to(blk_ref));
}

void cache::unlock_block(block *blk) {
    auto& blk_ref = reinterpret_cast<block&>(*blk);
    _lru.push_front(blk_ref);
}

size_t cache::block_size() {
    return _block_size;
}

float cache::get_hit_ratio() {
    return (float(_hits) / (_hits + _misses)) * 100.0;
}
