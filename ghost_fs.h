/*
  Ghost File System, or simply GhostFS
  Copyright (C) 2016 Raphael S. Carvalho

  This program can be distributed under the terms of the GNU GPL.
  See the file COPYING.
*/

#ifndef GHOST_FS_H
#define GHOST_FS_H

#define FUSE_USE_VERSION 26

#include <fuse.h>

#include "ghost_file.h"
#include "cache.h"

#ifndef ENOATTR
#define ENOATTR ENODATA /* Attribute not found */
#endif

#define BLOCK_SIZE (1024*1024)
#define CACHE_SIZE 1024

struct data_info {
    void *data;
    size_t offset;
    size_t size;
};

struct ghost_fs {
private:
    // TODO: introduce comparator method to optimize find.
    std::unordered_map<std::string, ghost_file> _files;
    cache _c;
public:
    // TODO: add option for the user to set cache settings.
    ghost_fs();

    void add_file(const char* file_path, const char* content);

    void add_file(const char* file_path);

    void remove_file(const char* file_path);

    bool file_exists(const char* file_path);

    std::unordered_map<std::string, ghost_file>& files();

    size_t get_block_size();

    cache& get_cache();
};

struct ghost_fs* get_ghost_fs();

//utility functions
void set_ghost_oper();
void add_static_files();

int ghost_main(int argc, char *argv[]);

#endif // GHOST_FS_H
