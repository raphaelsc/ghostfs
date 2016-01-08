/*
  Ghost File System, or simply GhostFS
  Copyright (C) 2016 Raphael S. Carvalho

  This program can be distributed under the terms of the GNU GPL.
  See the file COPYING.
*/

#include <curl/curl.h>

#include "utils.h"
#include "http_protocol.h"
#include "ghost_fs.h"

void http_protocol::get_block(const char *url, size_t block_id, size_t block_size,
        const std::unordered_map<std::string, std::string>& attributes, char* data) {
    char buffer[128];
    struct data_info info;
    CURL *curl;

    curl = curl_easy_init();
    if(!curl) {
        log("Curl initialization failed when about to get block %ld from %s\n", block_id, url);
    }

    curl_easy_setopt(curl, CURLOPT_URL, url);

    size_t offset = block_id * block_size;
    snprintf(buffer, 128, "%ld-%ld", offset, offset+block_size-1);
    log("\trange request to %s: %s\n", url, buffer);

    info.data = data;
    info.offset = 0;
    info.size = block_size;

    curl_easy_setopt(curl, CURLOPT_RANGE, buffer);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&info);

    /* Perform the request */
    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        log("Request to %s failed, reason: %s\n", url, curl_easy_strerror(res));
    }
    log("\tget_block finished for block %ld of %s!\n", block_id, url);
    curl_easy_cleanup(curl);
}

uint64_t http_protocol::get_content_length_for_url(const char *url) {
    CURL *curl = curl_easy_init();
    if(!curl) {
        log("Curl initialization failed when about to get length of %s\n", url);
        return -1;
    }

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_HEADER, 0);
    curl_easy_setopt(curl, CURLOPT_NOBODY, 1);

    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        log("Request to %s failed, reason: %s\n", url, curl_easy_strerror(res));
    }
    double content_length = 0;
    curl_easy_getinfo(curl, CURLINFO_CONTENT_LENGTH_DOWNLOAD, &content_length);
    curl_easy_cleanup(curl);
    return (uint64_t) content_length;
}

// TODO: check if web server exists and accepts range request
bool http_protocol::is_url_valid(const char* url) {
    return true;
}
