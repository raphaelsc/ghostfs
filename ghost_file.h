/*
  Ghost File System, or simply GhostFS
  Copyright (C) 2016 Raphael S. Carvalho

  This program can be distributed under the terms of the GNU GPL.
  See the file COPYING.
*/

#ifndef GHOST_FILE_H
#define GHOST_FILE_H

#include <unordered_map>
#include <vector>

#include "block_info.h"

struct ghost_file {
private:
    const char *_data;
    size_t _length;
    std::unordered_map<std::string, std::string> _attributes;
    std::vector<block_info> _file_blocks;
public:
    ghost_file(const char* data);

    ghost_file();

    const char* data() const;

    bool is_static() const;

    size_t length() const;

    void update_length(size_t new_length, size_t block_size);

    void add_attribute(const char* attribute, const char* value);

    void remove_attribute(const char* attribute);

    bool attribute_exists(const char* attribute) const;

    const std::unordered_map<std::string, std::string>& attributes() const;

    std::vector<block_info>& get_file_blocks();

    const char* get_url() const;
};

#endif // GHOST_FILE_H

