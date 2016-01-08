/*
  Ghost File System, or simply GhostFS
  Copyright (C) 2016 Raphael S. Carvalho

  This program can be distributed under the terms of the GNU GPL.
  See the file COPYING.
*/

#include "block_info.h"

void block_info::reset() {
    _present = false;
    _blk = nullptr;
}

void block_info::set_block(block *blk) {
    _present = true;
    _blk = blk;
}

block::block(block_info *info, char *data)
    : _info(info)
    , _data(data) {}
