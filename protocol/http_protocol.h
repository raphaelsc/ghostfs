/*
  Ghost File System, or simply GhostFS
  Copyright (C) 2016 Raphael S. Carvalho

  This program can be distributed under the terms of the GNU GPL.
  See the file COPYING.
*/

#ifndef HTTP_PROTOCOL_H
#define HTTP_PROTOCOL_H

#include "base_protocol.h"

struct http_protocol : public base_protocol {
    virtual const char* name() { return "http"; }

    virtual bool is_url_valid(const char* url);
    virtual uint64_t get_content_length_for_url(const char *url);
    virtual void get_block(const char *url, size_t block_id,
                                             size_t block_size, char* data);
};

struct https_protocol : public http_protocol {
    virtual const char* name() { return "https"; }
};

struct file_protocol : public http_protocol {
    virtual const char* name() { return "file"; }
};

#endif // HTTP_PROTOCOL_H
