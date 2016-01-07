/*
  Ghost File System, or simply GhostFS
  Copyright (C) 2015 Raphael S. Carvalho

  This program can be distributed under the terms of the GNU GPL.
  See the file COPYING.

  g++ --std=c++11 -Wall `pkg-config fuse --cflags --libs` ghost.cc -o ghost
*/

#define FUSE_USE_VERSION 26

#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unordered_map>
#include <string>
#include <sys/types.h>
#include <sys/xattr.h>
#include <curl/curl.h>
#include <vector>
#include <boost/intrusive/unordered_set.hpp>
#include <boost/intrusive/list.hpp>
#include <assert.h>
#include <mutex>
#include <thread>

#ifndef ENOATTR
#define ENOATTR ENODATA /* Attribute not found */
#endif

#define BLOCK_SIZE (1024*1024)
#define CACHE_SIZE 1024

namespace bi = boost::intrusive;

struct block_info;

struct block {
    block_info* _info;
    char* _data;
    bi::list_member_hook<> _lru_link;

    block(block_info* info, char* data)
        : _info(info)
        , _data(data) {}

    block() = delete;
};

struct block_info {
    bool _present = false;
    block* _blk = nullptr;
    std::mutex _mtx;

    block_info() = default;
    block_info(block_info&&) {}

    void reset() {
        _present = false;
        _blk = nullptr;
    }

    void set_block(block *blk) {
        _present = true;
        _blk = blk;
    }
};

struct ghost_file {
private:
    const char *_data;
    size_t _length;
    std::unordered_map<std::string, std::string> _attributes;
    std::vector<block_info> _file_blocks;
public:
    ghost_file(const char* data)
        : _data(data)
        , _length(strlen(data)) {}

    ghost_file()
        : _data(nullptr)
        , _length(0) {}

    const char* data() const {
        return _data;
    }

    bool is_static() const {
        return _data != nullptr;
    }

    size_t length() const {
        return _length;
    }

    void update_length(size_t new_length, size_t block_size) {
        _length = new_length;
        printf("File length: %ld\n", new_length);

        auto blocks = new_length / block_size + 1;
        _file_blocks.resize(blocks);
    }

    void add_attribute(const char* attribute, const char* value) {
        _attributes.emplace(std::string(attribute), std::string(value));
    }

    void remove_attribute(const char* file_path) {
        _attributes.erase(std::string(file_path));
    }

    bool attribute_exists(const char* file_path) const {
        auto it = _attributes.find(std::string(file_path));
        return (it == _attributes.end()) ? false : true;
    }

    const std::unordered_map<std::string, std::string>& attributes() const {
        return _attributes;
    }

    std::vector<block_info>& get_file_blocks() {
        return _file_blocks;
    }

    const char* get_url() const {
        auto it = _attributes.find(std::string("url"));
        if (it == _attributes.end()) {
            return nullptr;
        }
        return it->second.data();
    }
};

struct cache {
private:
    bi::list<block, bi::member_hook<block, bi::list_member_hook<>,
        &block::_lru_link>> _lru;
    size_t _blocks_used = 0;
    size_t _blocks_available;
    size_t _block_size;

    size_t blocks_used() {
        return _blocks_used;
    }
public:
    std::mutex _mtx;

    cache(size_t blocks, size_t block_size)
        : _blocks_available(blocks)
        , _block_size(block_size) {}

    ~cache() {
        _lru.clear();
    }

    // Return a block which isn't inserted to lru yet, i.e. the block is locked.
    block* allocate_block(block_info* info) {
        assert(_lru.size() <= _blocks_available);
        assert(blocks_used() <= _blocks_available);
        assert(info->_present == false);
        assert(info->_blk == nullptr);

        block *blk = nullptr;

        if (blocks_used() < _blocks_available) {
            char *data = new char[_block_size];

            blk = new block(info, data);
            // Make block_info of the caller store allocated block.
            info->set_block(blk);

            //_lru.push_front(reinterpret_cast<block&>(*blk));
            _blocks_used++;
        } else {
            // XXX: possible race condition with eviction.
            // If we evict a block which is about to be used but wasn't locked yet.

            assert(!_lru.empty());

            block& victim = reinterpret_cast<block&>(_lru.back());
            _lru.erase(_lru.iterator_to(victim)); // lock evicted block.
            assert(victim._info);

            // Reset block_info of the evicted block.
            victim._info->reset();
            // Make block_info of the caller store evicted block.
            info->set_block(&victim);
            // Make evicted block store block_info of the caller.
            victim._info = info;
            blk = &victim;
        }

        return blk;
    }

    void lock_block(block* blk) {
        auto& blk_ref = reinterpret_cast<block&>(*blk);
        _lru.erase(_lru.iterator_to(blk_ref));
    }

    void unlock_block(block* blk) {
        auto& blk_ref = reinterpret_cast<block&>(*blk);
        _lru.push_front(blk_ref);
    }

    size_t block_size() {
        return _block_size;
    }

    friend struct ghost_fs;
};

struct ghost_fs {
private:
    // TODO: introduce comparator method to optimize find.
    std::unordered_map<std::string, ghost_file> _files;
    cache _c;
public:
    // TODO: add option for the user to set cache settings.
    ghost_fs()
        : _c(CACHE_SIZE, BLOCK_SIZE) {}

    void add_file(const char* file_path, const char* content) {
        _files.emplace(std::string(file_path), ghost_file(content));
    }

    void add_file(const char* file_path) {
        _files.emplace(std::string(file_path), ghost_file());
    }

    void remove_file(const char* file_path) {
        _files.erase(std::string(file_path));
    }

    bool file_exists(const char* file_path) {
        auto it = _files.find(std::string(file_path));
        return (it == _files.end()) ? false : true;
    }

    std::unordered_map<std::string, ghost_file>& files() {
        return _files;
    }

    size_t get_block_size() {
        return _c.block_size();
    }

    cache& get_cache() {
        return _c;
    }
};

static struct ghost_fs* get_ghost_fs() {
    struct fuse_context* context = fuse_get_context();
    return (struct ghost_fs*) context->private_data;
}

struct data_info {
    void *data;
    size_t offset;
    size_t size;
};

static size_t write_callback(void *content_read, size_t size, size_t nmemb, void *p)
{
    size_t actual_size = size * nmemb;
    struct data_info* info = (struct data_info*) p;
    printf("\twrite_callback: actual_size: %ld\n", actual_size);

    if (info->offset + actual_size > info->size) {
        printf("\twrite_callback: write would go beyond the end of the buffer\n");
        return 0;
    }
    memcpy((char *) info->data + info->offset, content_read, actual_size);
    info->offset += actual_size;
    return actual_size;
}

static void get_block_from_remote_file(const char *url, size_t block_id, size_t block_size, char* data) {
    char buffer[128];
    struct data_info info;
    CURL *curl;

    curl = curl_easy_init();
    if(!curl) {
        printf("Failure\n");
    }

    curl_easy_setopt(curl, CURLOPT_URL, url);

    size_t offset = block_id * block_size;
    snprintf(buffer, 128, "%ld-%ld", offset, offset+block_size-1);
    printf("\trange: %s\n", buffer);

    info.data = data;
    info.offset = 0;
    info.size = block_size;

    curl_easy_setopt(curl, CURLOPT_RANGE, buffer);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&info);

    /* Perform the request */
    int res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        printf("Request failed\n");
    }
    printf("\tremote_read finished!\n");
    curl_easy_cleanup(curl);
}

static uint64_t get_content_length_for_url(const char *url)
{
    CURL *curl = curl_easy_init();
    if(!curl) {
        printf("Failure\n");
        return -1;
    }

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_HEADER, 0);
    curl_easy_setopt(curl, CURLOPT_NOBODY, 1);

    int res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        printf("Request failed\n");
    }
    double content_length = 0;
    curl_easy_getinfo(curl, CURLINFO_CONTENT_LENGTH_DOWNLOAD, &content_length);
    curl_easy_cleanup(curl);
    return (uint64_t) content_length;
}

// TODO: check if web server exists and accepts range request
static bool is_url_valid(const char* url)
{
    return true;
}

static int ghost_getattr(const char *path, struct stat *stbuf)
{
    struct ghost_fs* ghost = get_ghost_fs();
    int res = 0;

    memset(stbuf, 0, sizeof(struct stat));
    if (strcmp(path, "/") == 0) {
            stbuf->st_mode = S_IFDIR | 0755;
            stbuf->st_nlink = 2;
    } else {
        auto& files = ghost->files();
        auto it = files.find(path);
        if (it == files.end()) {
            res = -ENOENT;
        } else {
            stbuf->st_mode = S_IFREG | 0444;
            stbuf->st_nlink = 1;
            stbuf->st_size = it->second.length();
        }
    }

    return res;
}

static int ghost_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                         off_t offset, struct fuse_file_info *fi)
{
    struct ghost_fs* ghost = get_ghost_fs();

    (void) offset;
    (void) fi;

    // Root is the unique directory supported so far.
    if (strcmp(path, "/") != 0) {
        return -ENOENT;
    }

    filler(buf, ".", NULL, 0);
    filler(buf, "..", NULL, 0);

    for (auto& it : ghost->files()) {
        filler(buf, it.first.data() + 1, NULL, 0);
    }

    return 0;
}

static int ghost_open(const char *path, struct fuse_file_info *fi)
{
    struct ghost_fs* ghost = get_ghost_fs();

    printf("fi=%p, path=%s\n", fi, path);

    if (!ghost->file_exists(path)) {
        return -ENOENT;
    }

    if ((fi->flags & 3) != O_RDONLY) {
        return -EACCES;
    }

    return 0;
}

static int ghost_create(const char *path, mode_t mode, struct fuse_file_info *fi)
{
    struct ghost_fs* ghost = get_ghost_fs();
    bool exclusive = fi->flags & O_EXCL; // CHECK if it's correct

    //printf("CREATE: path=%s, create=%d, rdonly=%d, exclusive=%d\n", path, create, read_only, exclusive);
#if 0 /* Otherwise, command touch will not work */
    if ((fi->flags & 3) != O_RDONLY) {
        return -EACCES;
    }
#endif

    if (ghost->file_exists(path)) {
        if (exclusive) {
            return -EEXIST;
        }
    } else {
        ghost->add_file(path);
    }

    return 0;
}

static void do_prefetch(cache& c, block_info& info, size_t blk_id, std::string file_url) {
    get_block_from_remote_file(file_url.data(), blk_id, c.block_size(), info._blk->_data);
    c.unlock_block(info._blk);
    info._mtx.unlock();
    printf("Prefetched block %ld\n", blk_id);
}

static void try_prefetch(cache& c, block_info& info, size_t blk_id, const char* file_url) {
    if (!info._mtx.try_lock()) {
        return;
    }
    c._mtx.lock();

    if (info._present) {
        c._mtx.unlock();
        info._mtx.unlock();
        return;
    }
    printf("Prefetching block %ld\n", blk_id);
    block* blk = c.allocate_block(&info);
    c._mtx.unlock();
    assert(info._blk == blk);

    std::thread t(do_prefetch, std::ref(c), std::ref(info), blk_id, std::string(file_url));
    t.detach();
}

static int ghost_read(const char *path, char *buf, size_t size, off_t offset,
                      struct fuse_file_info *fi)
{
    struct ghost_fs* ghost = get_ghost_fs();

    auto& files = ghost->files();
    auto it = files.find(path);
    if (it == files.end()) {
        return -ENOENT;
    }
    auto& file = it->second;

    size_t len = file.length();
    if (offset < len) {
        if (offset + size > len) {
            size = len - offset;
        }
    } else {
        return 0;
    }

    if (file.is_static()) {
        memcpy(buf, file.data() + offset, size);
        return size;
    }

    const char *file_url = file.get_url();
    if (!file_url) {
        return 0;
    }
    printf("\nURL: %s\n", file_url);

    std::vector<block_info>& file_blocks = file.get_file_blocks();
    cache& c = ghost->get_cache();
    size_t end = offset + size;
    size_t buf_offset = 0;
    size_t block_size = ghost->get_block_size();
    size_t blk_id;

    while (offset < end) {
        blk_id = offset / block_size;
        size_t blk_offset = offset % block_size;
        size_t remaining = end - offset;
        size_t to_read = std::min(remaining, block_size - blk_offset);

        printf("blk_id=%ld, blk_offset=%ld, to_read=%ld\n", blk_id, blk_offset, to_read);

        block_info& info = file_blocks[blk_id];
        block* blk = nullptr;


        info._mtx.lock();
        c._mtx.lock();

        if (!info._present) {
            printf("\tnot cached\n");
            blk = c.allocate_block(&info);
            c._mtx.unlock();
            get_block_from_remote_file(file_url, blk_id, block_size, blk->_data);
        } else {
            printf("\tcached\n");
            blk = info._blk;
            c.lock_block(blk);
            c._mtx.unlock();
        }
        assert(info._blk->_info == &info);
        assert(info._blk->_data == blk->_data);

        assert(buf_offset + to_read <= size);
        memcpy(buf + buf_offset, blk->_data + blk_offset, to_read);

        c.unlock_block(blk);
        info._mtx.unlock();

        buf_offset += to_read;
        offset += to_read;
    }

    // Try to prefetch subsequent block.
    if ((blk_id + 1) < file_blocks.size()) {
        try_prefetch(c, file_blocks[blk_id+1], blk_id+1, file_url);
    }

    return size;
}

int ghost_setxattr(const char *path, const char *name, const char *value, size_t size, int flags) {
    char value_buf[size+1];
    memcpy((void*) &value_buf, value, size);
    value_buf[size] = '\0';

    printf("* setxattr: path=%s, name=%s, value=%s, size=%ld\n", path, name, value_buf, size);
    struct ghost_fs* ghost = get_ghost_fs();

    auto& files = ghost->files();
    auto it = files.find(path);
    if (it == files.end()) {
        return -ENOENT;
    }
    auto& file = it->second;

    if ((flags & XATTR_CREATE) && file.attribute_exists(name)) {
        return -EEXIST;
    } else if ((flags & XATTR_REPLACE) && !file.attribute_exists(name)) {
        return -ENOATTR;
    }

    // WARNING: if url attribute gets replaced, we need to invalidate all cache entries of the file and update file size
    file.add_attribute(name, value_buf);

    // Need to check if URL accepts range request, if not, we need to do something.
    if (strcmp(name, "url") == 0 && is_url_valid(value_buf)) {
        file.update_length(get_content_length_for_url(value_buf), ghost->get_block_size());
        std::vector<block_info>& file_blocks = file.get_file_blocks();
        cache& c = ghost->get_cache();
        try_prefetch(c, file_blocks[0], 0, value_buf);
    }

    return 0;
}

int ghost_getxattr(const char *path, const char *name, char *value, size_t size) {
    printf("* getxattr: path=%s, name=%s, value=%p, %s, size=%ld\n", path, name, value, value, size);
    struct ghost_fs* ghost = get_ghost_fs();

    auto& files = ghost->files();
    auto it = files.find(path);
    if (it == files.end()) {
        return -ENOENT;
    }
    auto& file = it->second;

    auto& attributes = file.attributes();
    auto it2 = attributes.find(name);
    if (it2 == attributes.end()) {
        return -ENOATTR;
    }
    auto& attribute_value = it2->second;
    size_t attribute_value_size = attribute_value.size();

    printf("\tattribute=%s, attribute_value_size=%ld\n", attribute_value.data(), attribute_value_size);

    if (size > 0) {
        // Size of buffer is small to hold the result.
        if (attribute_value_size > size) {
            return -ERANGE;
        }
        memcpy(value, attribute_value.data(), attribute_value_size);
    }

    return attribute_value_size;
}

int ghost_removexattr(const char *path, const char *name) {
    struct ghost_fs* ghost = get_ghost_fs();

    auto& files = ghost->files();
    auto it = files.find(path);
    if (it == files.end()) {
        return -ENOENT;
    }
    auto& file = it->second;

    if (!file.attribute_exists(name)) {
        return -ENOATTR;
    }
    file.remove_attribute(name);
    return 0;
}

static struct fuse_operations ghost_oper;
static struct ghost_fs ghost;

static void add_static_files() {
    static const char *ghost_path = "/HELLO";
    static const char *ghost_str = "Hello World!\n";
    ghost.add_file(ghost_path, ghost_str);

    static const char *credits_path = "/CREDITS";
    static const char *credits_str = "Raphael S. Carvalho <raphael.scarv@gmail.com>\n";
    ghost.add_file(credits_path, credits_str);
}

int main(int argc, char *argv[])
{
    ghost_oper.getattr = ghost_getattr;
    ghost_oper.readdir = ghost_readdir;
    ghost_oper.open = ghost_open;
    ghost_oper.create = ghost_create;
    ghost_oper.read = ghost_read;
    ghost_oper.setxattr = ghost_setxattr;
    ghost_oper.getxattr = ghost_getxattr;
    ghost_oper.removexattr = ghost_removexattr;

    add_static_files();

    return fuse_main(argc, argv, &ghost_oper, (void*) &ghost);
}
