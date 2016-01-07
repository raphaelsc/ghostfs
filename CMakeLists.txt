cmake_minimum_required(VERSION 2.8.3)

#Define cmake modules location

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/modules/")

#Use ccache if available

find_program(CCACHE_FOUND ccache)
if(CCACHE_FOUND)
    message(STATUS "Using ccache")
    set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE ccache)
    set_property(GLOBAL PROPERTY RULE_LAUNCH_LINK ccache)
endif(CCACHE_FOUND)

#Find dependencies

find_package(fuse REQUIRED)
find_package(CURL REQUIRED)

set(GHOST_LIBRARIES
	${FUSE_LIBRARIES}
	${CURL_LIBRARIES}
)

#Define compilation flags

set(CMAKE_CXX_FLAGS         "${CMAKE_CXX_FLAGS} --std=c++11 -Wall -pthread -D_FILE_OFFSET_BITS=64")
set(CMAKE_CXX_FLAGS_DEBUG   "${CMAKE_CXX_FLAGS} ${CMAKE_CXX_FLAGS_DEBUG} -fno-inline -ggdb -DDEBUG --std=c++11 -Wall")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS} ${CMAKE_CXX_FLAGS_RELEASE} -O2 -DNDEBUG --std=c++11 -Wall")

#Configure instalation settings

set(CMAKE_INSTALL_PATH /usr/bin)

#Add source codes and generate executables

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

install(
TARGETS ghostfs 
DESTINATION "${CMAKE_INSTALL_PATH}" 
COMPONENT application
)

install(
FILES ${CMAKE_CURRENT_SOURCE_DIR}/gmount
DESTINATION "${CMAKE_INSTALL_PATH}"
PERMISSIONS
	OWNER_READ OWNER_WRITE OWNER_EXECUTE
	GROUP_READ GROUP_WRITE GROUP_EXECUTE
	WORLD_READ WORLD_WRITE WORLD_EXECUTE
)

