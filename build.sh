#!/bin/bash

g++ --std=c++11 -Wall `pkg-config fuse --cflags --libs` ghostfs.cc -o ghost -lcurl

