/*
  Ghost File System, or simply GhostFS
  Copyright (C) 2016 Péricles Lopes Machado

  This program can be distributed under the terms of the GNU GPL.
  See the file COPYING.
*/

#include <dlfcn.h>
#include <vector>

#include "utils.h"
#include "protocol/load_drivers.h"
#include "protocol/base_protocol.h"


ghostfs_driver_init_func get_driver_init_function(const char* lib) {
    void* handler = dlopen(lib, RTLD_NOW);
    return reinterpret_cast<ghostfs_driver_init_func>(dlsym(handler, GHOSTFS_DRIVER_INIT_FUNC));
}

void register_drivers(const boost::filesystem::path& lib) {
    auto fini = get_driver_init_function(lib.string().c_str());

    if (fini) {
        fini();
    }
}

void find_drivers(const boost::filesystem::path& p) {
    namespace fs = boost::filesystem;

    fs::directory_iterator it(p);
    fs::directory_iterator end;

    for (; it != end; ++it) {
        if (fs::is_regular_file(it->path())) {
            auto f = it->path();
            auto fe = f.extension();

            if (fe.string() == GHOSTFS_DRIVER_EXT) {
                register_drivers(f);
            }
        }
    }
}

void load_drivers(const boost::filesystem::path& current_path) {
    namespace fs = boost::filesystem;

    std::vector<std::string> paths = {"/usr/bin",
                                 "/usr/lib",
                                 "/usr/local/bin",
                                 "/usr/local/lib",
                                 "/opt/bin",
                                 "/opt/lib"};

    paths.push_back(current_path.string());

    for (auto path : paths) {
        std::string l(path + "/" + GHOSTFS_DRIVER_DIR);

        if (boost::filesystem::is_directory(l)) {
            fs::path p(l);
            find_drivers(p);
        }
    }
}
