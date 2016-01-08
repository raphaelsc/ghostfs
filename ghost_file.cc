/*
  Ghost File System, or simply GhostFS
  Copyright (C) 2016 Raphael S. Carvalho

  This program can be distributed under the terms of the GNU GPL.
  See the file COPYING.
*/

#include "ghost_file.h"
#include "utils.h"

ghost_file::ghost_file(const char *data)
    : _data(data)
    , _length(strlen(data)) {}

ghost_file::ghost_file()
    : _data(nullptr)
    , _length(0) {}

const char *ghost_file::data() const {
    return _data;
}

bool ghost_file::is_static() const {
    return _data != nullptr;
}

size_t ghost_file::length() const {
    return _length;
}

void ghost_file::update_length(size_t new_length, size_t block_size) {
    _length = new_length;
    log("File length: %ld\n", new_length);

    auto blocks = new_length / block_size + 1;
    _file_blocks.resize(blocks);
}

void ghost_file::add_attribute(const char *attribute, const char *value) {
    _attributes.emplace(std::string(attribute), std::string(value));
}

void ghost_file::remove_attribute(const char *attribute) {
    _attributes.erase(std::string(attribute));
}

bool ghost_file::attribute_exists(const char *attribute) const {
    auto it = _attributes.find(std::string(attribute));
    return (it == _attributes.end()) ? false : true;
}

const std::unordered_map<std::string, std::string> &ghost_file::attributes() const {
    return _attributes;
}

std::vector<block_info> &ghost_file::get_file_blocks() {
    return _file_blocks;
}

const char *ghost_file::get_url() const {
    auto it = _attributes.find(std::string("url"));
    if (it == _attributes.end()) {
        return nullptr;
    }
    return it->second.data();
}
