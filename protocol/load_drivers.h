/*
  Ghost File System, or simply GhostFS
  Copyright (C) 2016 Raphael S. Carvalho

  This program can be distributed under the terms of the GNU GPL.
  See the file COPYING.
*/

// author: Péricles Lopes Machado
// Driver management

#ifndef LIB_PROTOCOL_H
#define LIB_PROTOCOL_H

#include <string>
#include <boost/filesystem.hpp>

void load_drivers(const boost::filesystem::path& current_path);

#define GHOSTFS_EXT_LIB_DECL extern "C"
#define GHOSTFS_DRIVER_DIR "ghostfs_driver"
#define GHOSTFS_DRIVER_INIT_FUNC "drivers_init"
#define GHOSTFS_DRIVER_EXT ".so"

typedef void (*ghostfs_driver_init_func)();

#endif // LIB_PROTOCOL_H
