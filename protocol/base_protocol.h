/*
  Ghost File System, or simply GhostFS
  Copyright (C) 2016 Raphael S. Carvalho

  This program can be distributed under the terms of the GNU GPL.
  See the file COPYING.
*/

#ifndef BASE_PROTOCOL_H
#define BASE_PROTOCOL_H

#include <stdint.h>

struct base_protocol {
    virtual const char* name() = 0;

    virtual bool is_url_valid(const char* url) = 0;
    virtual uint64_t get_content_length_for_url(const char *url) = 0;
    virtual void get_block(const char *url, size_t block_id,
                                             size_t block_size, char* data) = 0;
};

size_t write_callback(void *content_read, size_t size, size_t nmemb, void *p);

struct base_protocol* get_handler(const char* path);
void register_handler(struct base_protocol *handler);

#endif // BASE_PROTOCOL_H
