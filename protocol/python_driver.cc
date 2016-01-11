/*
  Ghost File System, or simply GhostFS
  Copyright (C) 2016 Péricles Lopes Machado

  This program can be distributed under the terms of the GNU GPL.
  See the file COPYING.
*/

#include <vector>

#include <boost/filesystem.hpp>

#include "protocol/base_protocol.h"
#include "protocol/python_driver.h"
#include "utils.h"

namespace  fs = boost::filesystem;

struct python_protocol_placeholder_impl {
    python_protocol_placeholder_impl(PyObject* _instance)
        : _instance(_instance) {}

    ~python_protocol_placeholder_impl() {
        Py_XDECREF(_instance);
    }

    const char* name();

    bool is_url_valid(const char* url);
    uint64_t get_content_length_for_url(const char *url);
    void get_block(const char *url, size_t block_id, size_t block_size,
                   const std::unordered_map<std::string, std::string>& attributes, char* data);
private:
    PyObject* _instance = 0;
};
////////////////////////////////////////////////////////////////////////////
using plugin_names_list = std::vector<std::string>;

plugin_names_list get_plugins(const char* module) {
    python::ensure_gil_state ensure_gil;
    plugin_names_list plugins_list;

    PyObject* pModule = PyImport_ImportModule(module);

    if (pModule == NULL) {
        log("Error during python module loading.");
        PyErr_Print();
        PyErr_Clear();
        return plugins_list;
    }

    PyObject* pFunc = PyObject_GetAttrString(pModule, GHOSTFS_PYTHON_GET_DRIVERS_FUNC);
    if (pFunc == NULL) {
        Py_XDECREF(pModule);
        PyErr_Print();
        PyErr_Clear();
        return plugins_list;
    }

    PyObject* pPluginList = PyObject_CallObject(pFunc, NULL);
    if (PyList_Check(pPluginList) == 0) {
        Py_XDECREF(pFunc);
        Py_XDECREF(pModule);
        log("'get_drivers()' must to return a list.");
        PyErr_Print();
        PyErr_Clear();
        return plugins_list;
    }

    Py_ssize_t pPluginListSize = PyList_Size(pPluginList);

    for (Py_ssize_t i = 0; i < pPluginListSize; ++i) {
        PyObject* pPlugin = PyList_GetItem(pPluginList, i);

        if (PyString_Check(pPlugin) == 1) {
            const char* plugin_name = PyString_AsString(pPlugin);
            plugins_list.push_back(plugin_name);
        }
    }

    //BEGIN Unload python objects
    Py_XDECREF(pPluginList);
    Py_XDECREF(pFunc);
    //Py_XDECREF(pModule);
    //END Unload python objects

    return plugins_list;
}

const char* python_protocol_placeholder_impl::name()
{
    python::ensure_gil_state ensure_gil;
    PyObject* result = PyObject_CallMethod(_instance, "name", NULL, NULL);

    if (result == NULL) {
        PyErr_Print();
        PyErr_Clear();
        return "";
    }

    if (PyString_Check(result)) {
        return	PyString_AsString(result);
    }

    return "";
}

bool python_protocol_placeholder_impl::is_url_valid(const char *url)
{
    python::ensure_gil_state ensure_gil;
    bool ok = false;

    PyObject* result = PyObject_CallMethod(_instance,
                                           "is_url_valid", "(s)", url);

    if (result == NULL) {
        PyErr_Print();
        PyErr_Clear();
        return false;
    }

    if (PyBool_Check(result)) {
        ok = result == Py_True;
    } else {
        Py_XDECREF(result);
    }

    return ok;
}

uint64_t python_protocol_placeholder_impl::get_content_length_for_url(const char *url)
{
    python::ensure_gil_state ensure_gil;
    PyObject* result = PyObject_CallMethod(_instance,
                                           "get_content_length_for_url", "(s)", url);

    if (result == NULL) {
        PyErr_Print();
        PyErr_Clear();
        return false;
    }

    uint64_t size = 0;

    if (PyLong_Check(result)) {
        size = PyLong_AsLongLong(result);
    } else if (PyInt_Check(result)) {
        size = PyInt_AsSsize_t(result);
    }

    Py_XDECREF(result);

    return size;
}

PyObject* convert_to_pyhashmap(const std::unordered_map<std::string, std::string> &attributes) {
    PyObject* dict = PyDict_New();

    for (auto it: attributes) {
        PyObject* py_key = PyString_FromString(it.first.c_str());
        PyObject* py_value = PyString_FromString(it.second.c_str());

        PyDict_SetItem(dict, py_key, py_value);

    }

    return dict;
}

void python_protocol_placeholder_impl::get_block(const char *url,
                                                 size_t block_id,
                                                 size_t block_size,
                                                 const std::unordered_map<std::string, std::string> &attributes,
                                                 char *data) {
    python::ensure_gil_state ensure_gil;
    size_t offset = block_id * block_size;


    PyObject* m = convert_to_pyhashmap(attributes);
    PyObject* result = PyObject_CallMethod(_instance,
                                           "get_block", "(sOLL)",
                                           url, m,
                                           offset, block_size);

    if (result == NULL) {
        PyErr_Print();
        PyErr_Clear();
        return;
    }

    size_t size = PyString_Size(result);
    char* c_result = PyString_AsString(result);

    if (c_result) {
        memcpy(data, c_result, size);
    }

    Py_XDECREF(m);
    Py_XDECREF(result);
}

///////////////////////////////////////////////////////////////////////////////////
python_protocol_placeholder::python_protocol_placeholder(PyObject* instance)
    : _impl(new python_protocol_placeholder_impl(instance)) {

}

python_protocol_placeholder::~python_protocol_placeholder() {
    delete _impl;
}

const char *python_protocol_placeholder::name() {
    return _impl->name();
}

bool python_protocol_placeholder::is_url_valid(const char *url) {
    return _impl->is_url_valid(url);
}

uint64_t python_protocol_placeholder::get_content_length_for_url(const char *url) {
    return _impl->get_content_length_for_url(url);
}

void python_protocol_placeholder::get_block(const char *url,
                                            size_t block_id,
                                            size_t block_size,
                                            const std::unordered_map<std::string, std::string> &attributes,
                                            char *data) {
    _impl->get_block(url,
                     block_id,
                     block_size,
                     attributes,
                     data);
}

python_protocol_placeholder* get_python_plugin(const char* module,
                                               const char* plugin_name) {
    PyObject* pModule = PyImport_ImportModule(module);
    if (pModule == NULL) {
        log("Error during module loading.\n");
        return 0;
    }

    PyObject* pClass = PyObject_GetAttrString(pModule, plugin_name);
    if (pClass == NULL) {
        Py_XDECREF(pModule);
        log("Error during class loading.\n");
        return 0;
    }

    PyObject* pInstance = PyInstance_New(pClass, NULL, NULL);
    if (pInstance == NULL) {
        Py_XDECREF(pClass);
        Py_XDECREF(pModule);
        log("Error during instance creation.\n");
        return 0;
    }

    if (PyObject_HasAttrString(pInstance, "name") == 0) {
        Py_XDECREF(pInstance);
        Py_XDECREF(pClass);
        Py_XDECREF(pModule);
        log("'name()' method isn't defined.\n");
        return 0;
    }

    if (PyObject_HasAttrString(pInstance, "is_url_valid") == 0) {
        Py_XDECREF(pInstance);
        Py_XDECREF(pClass);
        Py_XDECREF(pModule);
        log("'is_url_valid()' method isn't defined.\n");
        return 0;
    }

    if (PyObject_HasAttrString(pInstance, "get_content_length_for_url") == 0) {
        Py_XDECREF(pInstance);
        Py_XDECREF(pClass);
        Py_XDECREF(pModule);
        log("'get_content_length_for_url()' method isn't defined.\n");
        return 0;
    }

    if (PyObject_HasAttrString(pInstance, "get_block") == 0) {
        Py_XDECREF(pInstance);
        Py_XDECREF(pClass);
        Py_XDECREF(pModule);
        log("'get_block()' method isn't defined.\n");
        return 0;
    }

    return new python_protocol_placeholder(pInstance);
}

void register_python_drivers(const boost::filesystem::path& script) {
    python::ensure_gil_state ensure_gil;

    std::string module = script.stem().string();
    plugin_names_list plugins_list = get_plugins(module.c_str());

    for (auto plugin : plugins_list) {
        python_protocol_placeholder* handler =
                get_python_plugin(module.c_str(), plugin.c_str());

        if (handler) {
            register_handler(handler);
        } else {
            PyErr_Print();
            PyErr_Clear();
        }
    }
}

void find_python_drivers(const boost::filesystem::path& p) {
    namespace fs = boost::filesystem;

    fs::directory_iterator it(p);
    fs::directory_iterator end;

    for (; it != end; ++it) {
        if (fs::is_regular_file(it->path())) {
            auto f = it->path();
            auto fe = f.extension();

            if (fe.string() == ".py") {
                register_python_drivers(f);
            }
        }
    }
}


void load_python_drivers(const boost::filesystem::path &current_path)
{
    python::ensure_gil_state ensure_gil;

    std::vector<std::string> paths = {"/usr/bin",
                                      "/usr/lib",
                                      "/usr/local/bin",
                                      "/usr/local/lib",
                                      "/opt/bin",
                                      "/opt/lib"};

    paths.push_back(current_path.string());

    for (auto path : paths) {
        std::string l(path + "/" + GHOSTFS_PYTHON_DRIVER_DIR);

        if (boost::filesystem::is_directory(l)) {
            std::string set_python_env = "import sys\n"
                                         "sys.path.append('" + l + "/')\n";
            PyRun_SimpleString(set_python_env.c_str());

            fs::path p(l);
            find_python_drivers(p);
        }
    }
}

///////////////////////////////////////////////////////////
python::initialize::initialize()
{
    PyEval_InitThreads();
    Py_Initialize();
    _mainstate = PyThreadState_Swap(NULL);
    PyEval_ReleaseLock();
}

python::initialize::~initialize()
{
    PyEval_AcquireLock();
    PyThreadState_Swap(_mainstate);
    Py_Finalize();
}

python::ensure_gil_state::ensure_gil_state()
{
    _state = PyGILState_Ensure();
}

python::ensure_gil_state::~ensure_gil_state()
{
    PyGILState_Release(_state);
}
