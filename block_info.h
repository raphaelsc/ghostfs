/*
  Ghost File System, or simply GhostFS
  Copyright (C) 2016 Raphael S. Carvalho

  This program can be distributed under the terms of the GNU GPL.
  See the file COPYING.
*/

#ifndef BLOCK_INFO_H
#define BLOCK_INFO_H

#include <boost/intrusive/unordered_set.hpp>
#include <boost/intrusive/list.hpp>

#include <mutex>

struct block_info;

namespace bi = boost::intrusive;

struct block {
    block_info* _info;
    char* _data;
    bi::list_member_hook<> _lru_link;

    block(block_info* info, char* data)
        : _info(info)
        , _data(data) {}

    block() = delete;
};

struct block_info {
    bool _present = false;
    block* _blk = nullptr;
    std::mutex _mtx;

    block_info() = default;
    block_info(block_info&&) {}

    void reset();

    void set_block(block *blk);
};

#endif // BLOCK_INFO_H


