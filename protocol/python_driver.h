/*
  Ghost File System, or simply GhostFS
  Copyright (C) 2016 Péricles Lopes Machado

  This program can be distributed under the terms of the GNU GPL.
  See the file COPYING.
*/

#ifndef PYTHON_DRIVER_H
#define PYTHON_DRIVER_H

#include <string>
#include <boost/filesystem.hpp>
#include <Python.h>

#include "base_protocol.h"

void load_python_drivers(const boost::filesystem::path& current_path);

#define GHOSTFS_PYTHON_DRIVER_DIR "ghostfs_driver/python"
#define GHOSTFS_PYTHON_GET_DRIVERS_FUNC "get_drivers"

struct python_protocol_placeholder_impl;
struct python_protocol_placeholder : public base_protocol {
    python_protocol_placeholder(PyObject* instance);

    virtual ~python_protocol_placeholder();

    virtual const char* name();

    virtual bool is_url_valid(const char* url);
    virtual uint64_t get_content_length_for_url(const char *url);
    virtual void get_block(const char *url, size_t block_id, size_t block_size,
                           const std::unordered_map<std::string, std::string>& attributes, char* data);
private:
    python_protocol_placeholder_impl* _impl = 0;
};

namespace python {
//===========================================
/** Python GIL controller
* based on:
*  http://stackoverflow.com/questions/29595222/multithreading-with-python-and-c-api
*  https://gist.github.com/baruchs/e8090d0451ab781a4e22
*/
// initialize and clean up python
class initialize
{
public:
    initialize();

    ~initialize();

private:
    PyThreadState *_mainstate;
};

class ensure_gil_state
{
public:
    ensure_gil_state();

    ~ensure_gil_state();

private:
    PyGILState_STATE _state;
};
}

#endif // PYTHON_DRIVER_H
