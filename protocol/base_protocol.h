/*
  Ghost File System, or simply GhostFS
  Copyright (C) 2016 Péricles Lopes Machado

  This program can be distributed under the terms of the GNU GPL.
  See the file COPYING.
*/

#ifndef BASE_PROTOCOL_H
#define BASE_PROTOCOL_H

#include <stdint.h>
#include <unordered_map>

struct base_protocol {
    virtual ~base_protocol(){}

    virtual const char* name() = 0;

    virtual bool is_url_valid(const char* url) = 0;
    virtual uint64_t get_content_length_for_url(const char *url) = 0;
    // Return number of bytes stored in data, which cannot be greater than block_size.
    // Otherwise there would be an overflow on data.
    virtual size_t get_block(const char *url, size_t block_id, size_t block_size,
        const std::unordered_map<std::string, std::string>& attributes, char* data) = 0;
};

// write_callback() may be called multiple times to fullfil a request,
// so data_info is needed to keep track of the offset in data.
struct data_info {
    void *data;
    size_t offset;
    size_t size;
};

size_t write_callback(void *content_read, size_t size, size_t nmemb, void *p);

struct base_protocol* get_handler(const char* path);
void register_handler(struct base_protocol *handler);

#endif // BASE_PROTOCOL_H
