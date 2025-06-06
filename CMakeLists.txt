# @file      CMakeLists.txt
# @brief     c-axon library CMakeLists file
#
# MIT License
#
# Copyright joelguittet and c-axon contributors
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.

# CMake minimum version
cmake_minimum_required(VERSION 3.14)

# Project name
project(c-axon)

# Project version
file(STRINGS ${CMAKE_CURRENT_SOURCE_DIR}/VERSION PROJECT_VER LIMIT_COUNT 1)
string(REPLACE "." ";" PROJECT_VER_LIST ${PROJECT_VER})
list(GET PROJECT_VER_LIST 0 PROJECT_VER_MAJOR)
list(GET PROJECT_VER_LIST 1 PROJECT_VER_MINOR)
list(GET PROJECT_VER_LIST 2 PROJECT_VER_PATCH)

# Use GNU installation directories
include(GNUInstallDirs)

# Additional flags
set(c_flags "${c_flags} -Os -ffunction-sections -Wall -fPIC")
set(linker_flags "${linker_flags} -Wl,-gc-sections")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fvisibility=hidden")

# Definitions
add_definitions(-DAXON_EXPORT_SYMBOLS -DAXON_API_VISIBILITY)

# CMake subdirectories
if(NOT TARGET amp)
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/lib/amp/CMakeLists.txt)
        add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/lib/amp)
    endif()
endif()

# List of sources
file(GLOB_RECURSE src ${CMAKE_CURRENT_SOURCE_DIR}/src/*.c)

# Add include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/lib/amp/include)
    include_directories(${CMAKE_CURRENT_SOURCE_DIR}/lib/amp/include)
endif()
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/lib/amp/lib/cJSON/cJSON.h)
    include_directories(${CMAKE_CURRENT_SOURCE_DIR}/lib/amp/lib/cJSON)
endif()

# Creation of the library
add_library(axon SHARED ${src})

# Link the library with the wanted libraries
target_link_libraries(axon amp cjson pthread rt)

# Properties of the library
set_target_properties(axon
    PROPERTIES
    SOVERSION "${PROJECT_VER_MAJOR}"
    VERSION "${PROJECT_VER_MAJOR}.${PROJECT_VER_MINOR}.${PROJECT_VER_PATCH}"
)

# Creation of the examples binaries
option(ENABLE_AXON_EXAMPLES "Enable building axon examples" OFF)
if(ENABLE_AXON_EXAMPLES)
    add_executable(pub ${CMAKE_CURRENT_SOURCE_DIR}/examples/pubsub/pub.c)
    target_link_libraries(pub amp axon rt)
    add_executable(sub ${CMAKE_CURRENT_SOURCE_DIR}/examples/pubsub/sub.c)
    target_link_libraries(sub amp axon rt)
    add_executable(pub_topic1_topic2 ${CMAKE_CURRENT_SOURCE_DIR}/examples/pubsub_topics/pub_topic1_topic2.c)
    target_link_libraries(pub_topic1_topic2 amp axon rt)
    add_executable(sub_topic1_topic2 ${CMAKE_CURRENT_SOURCE_DIR}/examples/pubsub_topics/sub_topic1_topic2.c)
    target_link_libraries(sub_topic1_topic2 amp axon rt)
    add_executable(sub_topic1 ${CMAKE_CURRENT_SOURCE_DIR}/examples/pubsub_topics/sub_topic1.c)
    target_link_libraries(sub_topic1 amp axon rt)
    add_executable(sub_topic2 ${CMAKE_CURRENT_SOURCE_DIR}/examples/pubsub_topics/sub_topic2.c)
    target_link_libraries(sub_topic2 amp axon rt)
    add_executable(sub_topics ${CMAKE_CURRENT_SOURCE_DIR}/examples/pubsub_topics/sub_topics.c)
    target_link_libraries(sub_topics amp axon rt)
    add_executable(push ${CMAKE_CURRENT_SOURCE_DIR}/examples/pushpull/push.c)
    target_link_libraries(push amp axon rt)
    add_executable(pull ${CMAKE_CURRENT_SOURCE_DIR}/examples/pushpull/pull.c)
    target_link_libraries(pull amp axon rt)
    add_executable(rep ${CMAKE_CURRENT_SOURCE_DIR}/examples/reqrep/rep.c)
    target_link_libraries(rep amp axon rt)
    add_executable(req ${CMAKE_CURRENT_SOURCE_DIR}/examples/reqrep/req.c)
    target_link_libraries(req amp axon rt)
endif()

# Installation
set(CMAKE_INSTALL_FULL_LIBDIR lib)
set(CMAKE_INSTALL_FULL_BINDIR bin)
set(CMAKE_INSTALL_FULL_INCLUDEDIR include)
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/include/axon.h DESTINATION "${CMAKE_INSTALL_FULL_INCLUDEDIR}")
install(TARGETS axon
    ARCHIVE DESTINATION "${CMAKE_INSTALL_FULL_LIBDIR}"
    LIBRARY DESTINATION "${CMAKE_INSTALL_FULL_LIBDIR}"
    RUNTIME DESTINATION "${CMAKE_INSTALL_FULL_BINDIR}"
    INCLUDES DESTINATION "${CMAKE_INSTALL_FULL_INCLUDEDIR}"
)
if(ENABLE_AXON_EXAMPLES)
    install(TARGETS pub sub pub_topic1_topic2 sub_topic1_topic2 sub_topic1 sub_topic2 sub_topics push pull rep req
        ARCHIVE DESTINATION "${CMAKE_INSTALL_FULL_LIBDIR}"
        LIBRARY DESTINATION "${CMAKE_INSTALL_FULL_LIBDIR}"
        RUNTIME DESTINATION "${CMAKE_INSTALL_FULL_BINDIR}"
        INCLUDES DESTINATION "${CMAKE_INSTALL_FULL_INCLUDEDIR}"
    )
endif()
