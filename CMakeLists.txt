cmake_minimum_required(VERSION 2.8.3)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/modules/")

find_package(fuse REQUIRED)
find_package(CURL REQUIRED)

set(CMAKE_CXX_FLAGS         "${CMAKE_CXX_FLAGS} --std=c++11 -Wall -pthread -D_FILE_OFFSET_BITS=64")
set(CMAKE_CXX_FLAGS_DEBUG   "${CMAKE_CXX_FLAGS} ${CMAKE_CXX_FLAGS_DEBUG} -fno-inline -ggdb -DDEBUG --std=c++11 -Wall")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS} ${CMAKE_CXX_FLAGS_RELEASE} -O2 -DNDEBUG --std=c++11 -Wall")

set(GHOST_LIBRARIES
	${FUSE_LIBRARIES}
	${CURL_LIBRARIES}
)

include_directories(
	${FUSE_INCLUDE_DIRS}
	${CURL_INCLUDE_DIRS}
	${CMAKE_SOURCE_DIR}
)

add_executable(ghostfs
	ghostfs.cc
)

target_link_libraries(ghostfs 
	${GHOST_LIBRARIES}
)

