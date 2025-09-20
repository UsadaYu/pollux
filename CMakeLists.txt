cmake_minimum_required(VERSION 3.15)

set(POLLUX_VERSION_MAJOR 2)
set(POLLUX_VERSION_MINOR 0)
set(POLLUX_VERSION_PATCH 0)
set(POLLUX_VERSION
    "${POLLUX_VERSION_MAJOR}.${POLLUX_VERSION_MINOR}.${POLLUX_VERSION_PATCH}")

project(
  PROJECT-POLLUX
  VERSION ${POLLUX_VERSION}
  DESCRIPTION "pollux"
  LANGUAGES "C" "CXX"
  HOMEPAGE_URL "https://github.com/UsadaYu/pollux")

set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_C_STANDARD_REQUIRED ON)

# requirement
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/requirement.txt" _require_content)
macro(version_read VERSION_VARIABLE REQUIRE_CONTENT REGEX_PREFIX)
  string(REGEX MATCH "${REGEX_PREFIX}_VERSION=([0-9.]+)" _version_match
               "${REQUIRE_CONTENT}")
  if(_version_match)
    set(${VERSION_VARIABLE} "${CMAKE_MATCH_1}")
  endif()
endmacro()
version_read(_required_sirius_version "${_require_content}" "SIRIUS")
version_read(_required_avformat_version "${_require_content}" "FFMPEG_AVFORMAT")
version_read(_required_avcodec_version "${_require_content}" "FFMPEG_AVCODEC")
version_read(_required_avutil_version "${_require_content}" "FFMPEG_AVUTIL")
version_read(_required_swresample_version "${_require_content}"
             "FFMPEG_SWRESAMPLE")
version_read(_required_swscale_version "${_require_content}" "FFMPEG_SWSCALE")
unset(_require_content)

set(sirius_NAMESPACE
    "sirius"
    CACHE STRING "`sirius` namespace")
find_package(${sirius_NAMESPACE} ${_required_sirius_version})
if(NOT ${sirius_NAMESPACE}_FOUND)
  message(
    WARNING
      "Use `-DCMAKE_PREFIX_PATH` to specify the `cmake prefix` for `sirius`\n"
      "`sirius` source address: https://github.com/UsadaYu/sirius")
  return()
else()
  message(
    STATUS "The cmake config path for `sirius`: ${${sirius_NAMESPACE}_DIR}")
endif()

find_package(PkgConfig REQUIRED)

pkg_check_modules(
  PKG_FFMPEG
  QUIET
  libavformat>=${_required_avformat_version}
  libavcodec>=${_required_avcodec_version}
  libavutil>=${_required_avutil_version}
  libswresample>=${_required_swresample_version}
  libswscale>=${_required_swscale_version})
if(PKG_FFMPEG_FOUND)
  get_filename_component(_ffmpeg_prefix_dir ${PKG_FFMPEG_INCLUDE_DIRS}
                         DIRECTORY)
else()
  find_program(_ffmpeg_executable NAMES ffmpeg)
  if(_ffmpeg_executable)
    get_filename_component(_ffmpeg_bin_dir ${_ffmpeg_executable} DIRECTORY)
    get_filename_component(_ffmpeg_prefix_dir ${_ffmpeg_bin_dir} DIRECTORY)
    unset(_ffmpeg_bin_dir)
  else()
    message(
      WARNING
        "Use `-DFFMPEG_PKG_PREFIX` to specify the `pkgconfig` for `ffmpeg`\n"
        "`ffmpeg` source address: https://ffmpeg.org/download.html")
  endif()
  unset(_ffmpeg_executable)
endif()
set(FFMPEG_PKG_PREFIX
    "${_ffmpeg_prefix_dir}/lib/pkgconfig"
    CACHE STRING
          "The path prefix for `ffmpeg`, e.g., `/usr/local/lib/pkgconfig`")
unset(_ffmpeg_prefix_dir)
set(ENV{PKG_CONFIG_PATH} ${FFMPEG_PKG_PREFIX})
message(STATUS "The `PKG_CONFIG_PATH` for `ffmpeg`: $ENV{PKG_CONFIG_PATH}")
unset(PKG_FFMPEG_FOUND CACHE)
pkg_check_modules(
  PKG_FFMPEG
  REQUIRED
  libavformat>=${_required_avformat_version}
  libavcodec>=${_required_avcodec_version}
  libavutil>=${_required_avutil_version}
  libswresample>=${_required_swresample_version}
  libswscale>=${_required_swscale_version})

unset(_required_sirius_version)
unset(_required_avformat_version)
unset(_required_avcodec_version)
unset(_required_avutil_version)
unset(_required_swresample_version)
unset(_required_swscale_version)

# target information
set(POLLUX_TARGET_NAME
    "pollux"
    CACHE STRING "Target name")
set(TARGET_DIR "${PROJECT_BINARY_DIR}/artifact/${POLLUX_TARGET_NAME}")
set(TARGET_LIB_DIR "${TARGET_DIR}/lib")

add_subdirectory(cmake)

option(POLLUX_TEST_ENABLE "Enable test" OFF)
if(POLLUX_TEST_ENABLE)
  enable_testing()
  add_subdirectory(unittests)
endif()
