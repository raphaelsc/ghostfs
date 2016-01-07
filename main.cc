/*
  Ghost File System, or simply GhostFS
  Copyright (C) 2016 Raphael S. Carvalho

  This program can be distributed under the terms of the GNU GPL.
  See the file COPYING.

  g++ --std=c++11 -Wall `pkg-config fuse --cflags --libs` ghost.cc -o ghost -lcurl
*/

#include "cache.h"
#include "ghost_file.h"
#include "ghost_fs.h"

int main(int argc, char *argv[])
{
    set_ghost_oper();
    add_static_files();

    return ghost_main(argc, argv);
}
