/*
  Ghost File System, or simply GhostFS
  Copyright (C) 2016 Péricles Lopes Machado

  This program can be distributed under the terms of the GNU GPL.
  See the file COPYING.
*/

#include <string>

#include "utils.h"
#include "base_protocol.h"
#include "ghost_fs.h"

size_t write_callback(void *content_read, size_t size, size_t nmemb, void *p) {
    size_t actual_size = size * nmemb;
    struct data_info* info = (struct data_info*) p;
    log("\twrite_callback: actual_size: %ld\n", actual_size);

    // Handle possible overflow.
    if (info->offset + actual_size > info->size) {
        log("\twrite_callback: write would go beyond the end of the buffer\n");
        return 0;
    }
    memcpy((char *) info->data + info->offset, content_read, actual_size);
    info->offset += actual_size;
    return actual_size;
}

std::unordered_map<std::string, struct base_protocol*> handlers_;

void register_handler(struct base_protocol *handler) {
    handlers_[handler->name()] = handler;
}

std::string get_protocol(const std::string& path) {
    auto tokens = split(path, ':');

    if (tokens.size() == 0) return "";

    return tokens[0];
}

struct base_protocol *get_handler(const char *path) {
    auto it = handlers_.find(get_protocol(path));

    if (it == handlers_.end()) return 0;

    return it->second;
}
