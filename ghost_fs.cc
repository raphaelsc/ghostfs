/*
  Ghost File System, or simply GhostFS
  Copyright (C) 2016 Raphael S. Carvalho

  This program can be distributed under the terms of the GNU GPL.
  See the file COPYING.
*/

#define FUSE_USE_VERSION 26

#include <fuse.h>
#include <thread>
#include <boost/filesystem.hpp>

#include "ghost_fs.h"
#include "utils.h"

#include "protocol/http_protocol.h"
#include "protocol/load_drivers.h"
#include "protocol/python_driver.h"

ghost_fs *get_ghost_fs() {
    struct fuse_context* context = fuse_get_context();
    return (struct ghost_fs*) context->private_data;
}

ghost_fs::ghost_fs()
    : _c(CACHE_SIZE, BLOCK_SIZE) {}

void ghost_fs::add_file(const char *file_path, const char *content) {
    _files.emplace(std::string(file_path), ghost_file(content));
}

void ghost_fs::add_file(const char *file_path) {
    _files.emplace(std::string(file_path), ghost_file());
}

void ghost_fs::remove_file(const char *file_path) {
    _files.erase(std::string(file_path));
}

bool ghost_fs::file_exists(const char *file_path) {
    auto it = _files.find(std::string(file_path));
    return (it == _files.end()) ? false : true;
}

std::unordered_map<std::string, ghost_file> &ghost_fs::files() {
    return _files;
}

size_t ghost_fs::get_block_size() {
    return _c.block_size();
}

cache &ghost_fs::get_cache() {
    return _c;
}

// fuse handlers

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

    log("fi=%p, path=%s\n", fi, path);

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

    //log("CREATE: path=%s, create=%d, rdonly=%d, exclusive=%d\n", path, create, read_only, exclusive);
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

static void do_prefetch(cache& c, ghost_file& file, size_t blk_id, std::string file_url) {
    std::vector<block_info>& file_blocks = file.get_file_blocks();
    block_info& info = file_blocks[blk_id];
    base_protocol* handler = get_handler(file_url.data());

    if (!handler) return;

    // XXX: handle possible failure here, so we will have to deallocate block, provide
    // the function in cache.
    handler->get_block(file_url.data(), blk_id, c.block_size(),
        file.attributes(), info._blk->_data);

    c.unlock_block(info._blk);

    info._mtx.unlock();
    log("Prefetched block %ld\n", blk_id);
}

static void try_prefetch(cache& c, ghost_file& file, size_t blk_id, const char* file_url) {
    std::vector<block_info>& file_blocks = file.get_file_blocks();
    block_info& info = file_blocks[blk_id];

    if (!info._mtx.try_lock()) {
        return;
    }
    c._mtx.lock();

    if (info._present) {
        c._mtx.unlock();
        info._mtx.unlock();
        return;
    }
    log("Prefetching block %ld\n", blk_id);
    block* blk = c.allocate_block(&info);
    c._mtx.unlock();
    assert(info._blk == blk);

    std::thread t(do_prefetch, std::ref(c), std::ref(file), blk_id, std::string(file_url));
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

    log("\nURL: %s\n", file_url);

    base_protocol* handler = get_handler(file_url);

    if (!handler) return 0;

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

        log("blk_id=%ld, blk_offset=%ld, to_read=%ld\n", blk_id, blk_offset, to_read);

        block_info& info = file_blocks[blk_id];
        block* blk = nullptr;

        info._mtx.lock();
        c._mtx.lock();

        if (!info._present) {
            c._misses++;
            log("\tnot cached, hit ratio=%6.2f%%\n", c.get_hit_ratio());
            blk = c.allocate_block(&info);
            c._mtx.unlock();
            size_t bytes_read = handler->get_block(file_url, blk_id, block_size, file.attributes(), blk->_data);
            // If get_block() was unable to get the amount of bytes asked, then we should return EIO.
            if (bytes_read < to_read) {
                log("get_block failed for block %ld, expected=%ld, actual=%ld\n", blk_id, to_read, bytes_read);
                return -EIO;
            }
            // If bytes read is greater than block size, then there is an overflow in blk->_data
            assert(bytes_read <= block_size);
        } else {
            c._hits++;
            log("\tcached, hit ratio=%6.2f%%\n", c.get_hit_ratio());
            blk = info._blk;
            c.lock_block(blk);
            c._mtx.unlock();
        }
        assert(info._blk->_info == &info);
        assert(info._blk->_data == blk->_data);

        assert(buf_offset + to_read <= size);
        memcpy(buf + buf_offset, blk->_data + blk_offset, to_read);

        // TODO: Possibly, we will have to acquire cache mutex to unlock block.
        c.unlock_block(blk);
        info._mtx.unlock();

        buf_offset += to_read;
        offset += to_read;
    }

    // Try to prefetch subsequent block.
    if ((blk_id + 1) < file_blocks.size()) {
        try_prefetch(c, file, blk_id+1, file_url);
    }

    return size;
}

int ghost_setxattr(const char *path, const char *name,
                   const char *value, size_t size, int flags) {
    char value_buf[size+1];
    memcpy((void*) &value_buf, value, size);
    value_buf[size] = '\0';

    log("* setxattr: path=%s, name=%s, value=%s, size=%ld\n",
           path, name, value_buf, size);
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

    // WARNING: if url attribute gets replaced, we need to invalidate
    // all cache entries of the file and update file size
    file.add_attribute(name, value_buf);

    base_protocol* handler = get_handler(value_buf);

    if (!handler) return 0;

    // Need to check if URL accepts range request, if not, we need to do something.
    if (strcmp(name, "url") == 0 &&
            handler->is_url_valid(value_buf)) {
        file.update_length(handler->get_content_length_for_url(value_buf),
                           ghost->get_block_size());
        try_prefetch(ghost->get_cache(), file, 0, value_buf);
    }

    return 0;
}

int ghost_getxattr(const char *path, const char *name, char *value, size_t size) {
    log("* getxattr: path=%s, name=%s, value=%p, %s, size=%ld\n",
           path, name, value, value, size);
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

    log("\tattribute=%s, attribute_value_size=%ld\n",
           attribute_value.data(), attribute_value_size);

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


// Utility functions
struct fuse_operations ghost_oper;
struct ghost_fs ghost;

void set_ghost_oper() {
    ghost_oper.getattr = ghost_getattr;
    ghost_oper.readdir = ghost_readdir;
    ghost_oper.open = ghost_open;
    ghost_oper.create = ghost_create;
    ghost_oper.read = ghost_read;
    ghost_oper.setxattr = ghost_setxattr;
    ghost_oper.getxattr = ghost_getxattr;
    ghost_oper.removexattr = ghost_removexattr;
}

void add_static_files() {
    static const char *ghost_path = "/HELLO";
    static const char *ghost_str = "Hello World!\n";
    ghost.add_file(ghost_path, ghost_str);

    static const char *credits_path = "/CREDITS";
    static const char *credits_str = "Raphael S. Carvalho <raphael.scarv@gmail.com>\n";
    ghost.add_file(credits_path, credits_str);
}

void register_handlers() {
    register_handler(new http_protocol);
    register_handler(new https_protocol);
    register_handler(new file_protocol);
}

namespace fs = boost::filesystem;

int ghost_main(int argc, char *argv[]) {
    fs::path current_path = fs::system_complete(fs::path(argv[0])).parent_path();

    set_ghost_oper();
    add_static_files();
    register_handlers();

    //load extern drivers
    load_drivers(current_path);

    python::initialize initialize;
    load_python_drivers(current_path);

    return fuse_main(argc, argv, &ghost_oper, (void*) &ghost);
}
