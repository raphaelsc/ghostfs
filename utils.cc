/*
  Ghost File System, or simply GhostFS
  Copyright (C) 2016 Raphael S. Carvalho

  This program can be distributed under the terms of the GNU GPL.
  See the file COPYING.
*/

#include "utils.h"

#include <sstream>
#include <stdarg.h>

std::vector<std::string> &split(const std::string &s, char delim,
                                std::vector<std::string> &elems) {
    std::stringstream ss(s);
    std::string item;
    while (std::getline(ss, item, delim)) {
        elems.push_back(item);
    }
    return elems;
}


std::vector<std::string> split(const std::string &s, char delim) {
    std::vector<std::string> elems;
    split(s, delim, elems);
    return elems;
}

int log(const char *format, ...) {
    va_list args;
    va_start(args, format);

    int ret = vprintf(format, args);

    va_end(args);
    return ret;
}
