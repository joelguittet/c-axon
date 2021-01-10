# CMake minimum version
cmake_minimum_required(VERSION 3.14)

# Project name
project(c-axon)

# Project version
set(AXON_VERSION_MAJOR 2)
set(AXON_VERSION_MINOR 0)
set(AXON_VERSION_PATCH 3)

# Additional flags
set(c_flags "${c_flags} -Os -ffunction-sections -Wall -fPIC")
set(linker_flags "${linker_flags} -Wl,-gc-sections")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fvisibility=hidden")

# Definitions
add_definitions(-DAXON_EXPORT_SYMBOLS -DAXON_API_VISIBILITY)

# Output directories
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/build/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/build/bin)

# CMake subdirectories
if (NOT TARGET amp)
  if(EXISTS lib/amp)
    add_subdirectory(lib/amp)
  endif()
endif()
if(NOT TARGET cjson)
  if(EXISTS lib/cJSON)
    add_subdirectory(lib/cJSON)
  endif()
endif()

# List of sources
file(GLOB_RECURSE src "src/*.c")

# Add include directories
include_directories(inc)
if(EXISTS lib/amp)
  include_directories(lib/amp/inc)
endif()
if(EXISTS lib/cJSON)
  include_directories(lib/cJSON)
endif()

# Creation of the library
add_library(axon SHARED ${src})

# Link the library with the wanted libraries
target_link_libraries(axon amp cjson pthread rt)

# Properties of the library
set_target_properties(axon
  PROPERTIES
  SOVERSION "${AXON_VERSION_MAJOR}"
  VERSION "${AXON_VERSION_MAJOR}.${AXON_VERSION_MINOR}.${AXON_VERSION_PATCH}"
)

# Creation of the examples binaries
option(ENABLE_AXON_EXAMPLES "Enable building axon examples" OFF)
if(ENABLE_AXON_EXAMPLES)
  add_executable(pub "examples/pubsub/pub.c")
  target_link_libraries(pub amp axon rt)
  add_executable(sub "examples/pubsub/sub.c")
  target_link_libraries(sub amp axon rt)
  add_executable(pub_topic1_topic2 "examples/pubsub_topics/pub_topic1_topic2.c")
  target_link_libraries(pub_topic1_topic2 amp axon rt)
  add_executable(sub_topic1_topic2 "examples/pubsub_topics/sub_topic1_topic2.c")
  target_link_libraries(sub_topic1_topic2 amp axon rt)
  add_executable(sub_topic1 "examples/pubsub_topics/sub_topic1.c")
  target_link_libraries(sub_topic1 amp axon rt)
  add_executable(sub_topic2 "examples/pubsub_topics/sub_topic2.c")
  target_link_libraries(sub_topic2 amp axon rt)
  add_executable(sub_topics "examples/pubsub_topics/sub_topics.c")
  target_link_libraries(sub_topics amp axon rt)
  add_executable(push "examples/pushpull/push.c")
  target_link_libraries(push amp axon rt)
  add_executable(pull "examples/pushpull/pull.c")
  target_link_libraries(pull amp axon rt)
  add_executable(rep "examples/reqrep/rep.c")
  target_link_libraries(rep amp axon rt)
  add_executable(req "examples/reqrep/req.c")
  target_link_libraries(req amp axon rt)
endif()

# Installation
set(CMAKE_INSTALL_FULL_LIBDIR lib)
set(CMAKE_INSTALL_FULL_BINDIR bin)
set(CMAKE_INSTALL_FULL_INCLUDEDIR include)
if(EXISTS lib/cJSON)
  install(FILES lib/cJSON/cJSON.h DESTINATION "${CMAKE_INSTALL_FULL_INCLUDEDIR}")
  install(TARGETS cjson
    ARCHIVE DESTINATION "${CMAKE_INSTALL_FULL_LIBDIR}"
    LIBRARY DESTINATION "${CMAKE_INSTALL_FULL_LIBDIR}"
    RUNTIME DESTINATION "${CMAKE_INSTALL_FULL_BINDIR}"
    INCLUDES DESTINATION "${CMAKE_INSTALL_FULL_INCLUDEDIR}"
  )
endif()
if(EXISTS lib/amp)
  install(FILES lib/amp/inc/amp.h DESTINATION "${CMAKE_INSTALL_FULL_INCLUDEDIR}")
  install(TARGETS amp
    ARCHIVE DESTINATION "${CMAKE_INSTALL_FULL_LIBDIR}"
    LIBRARY DESTINATION "${CMAKE_INSTALL_FULL_LIBDIR}"
    RUNTIME DESTINATION "${CMAKE_INSTALL_FULL_BINDIR}"
    INCLUDES DESTINATION "${CMAKE_INSTALL_FULL_INCLUDEDIR}"
  )
endif()
install(FILES inc/axon.h DESTINATION "${CMAKE_INSTALL_FULL_INCLUDEDIR}")
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
