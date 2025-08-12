option(BUILD_SHARED_LIBS "Build shared libraries" OFF)

option(POLLUX_ASAN "Enable asan" OFF)

option(POLLUX_GCOV "Enable gcov" OFF)

option(POLLUX_WARNING_ALL "Enable all compile warnings" ON)

option(POLLUX_WARNING_ERROR "Compile warnings as errors" OFF)

if(NOT CMAKE_SYSTEM_NAME STREQUAL "Windows")
  option(POLLUX_PIC_ENABLE "Position independent, enable `pic`" ON)
endif()

set(
  POLLUX_MODULE_PRINT_NAME "libpollux"
  CACHE STRING
  "Module print name"
)

if(CMAKE_BUILD_TYPE STREQUAL "Debug" OR
  CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
  set(
    POLLUX_LOG_LEVEL "4"
    CACHE STRING
    "Log level: 0: disable the log; 1: error; 2: warn; 3: info; 4: debug"
  )
else()
  set(
    POLLUX_LOG_LEVEL "3"
    CACHE STRING
    "Log level: 0: disable the log; 1: error; 2: warn; 3: info; 4: debug"
  )
endif()

set(
  POLLUX_ENCODE_AV1_LIB ""
  CACHE STRING
  "`AV1` encoding library, `libstvav1`(default) or `libaom`"
)

set(
  POLLUX_EXTRA_LINK_DIR
  "" CACHE STRING
  "The directory of `libs` that require additional links to the `pollux`, e.g., `/path/lib1;/path/lib2`"
)

set(
  POLLUX_EXTRA_LINK_LIBRARIES
  "" CACHE STRING
  "The name of `libs` that require additional links to the `pollux`, e.g., `pthread;stdc++`"
)
