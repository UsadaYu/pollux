set(_src_list "")
set(_src_dir ${PROJECT_SOURCE_DIR}/src)
aux_source_directory(${_src_dir} _aux_src_list)
aux_source_directory(${_src_dir}/codec _aux_src_codec_list)
list(APPEND _src_list ${_aux_src_list})
list(APPEND _src_list ${_aux_src_codec_list})
unset(_aux_src_codec_list)
unset(_aux_src_list)

set(LIBRARY_OUTPUT_PATH ${TARGET_LIB_DIR})
if(BUILD_SHARED_LIBS)
  add_library(${POLLUX_TARGET_NAME} SHARED ${_src_list})

  if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    target_compile_definitions(${POLLUX_TARGET_NAME} PUBLIC ${POLLUX_WIN_DLL})
  endif()
else()
  add_library(${POLLUX_TARGET_NAME} STATIC ${_src_list})
endif()

if(NOT CMAKE_SYSTEM_NAME STREQUAL "Windows")
  if(POLLUX_PIC_ENABLE)
    target_compile_options(${POLLUX_TARGET_NAME} PRIVATE -fPIC)
  endif()

  target_compile_options(${POLLUX_TARGET_NAME} PRIVATE -fvisibility=hidden)
endif()

include(${CMAKE_CURRENT_SOURCE_DIR}/codec.cmake)

get_target_property(
  _sirius_compile_options ${sirius_NAMESPACE}::${sirius_NAMESPACE}
  INTERFACE_COMPILE_OPTIONS)
if(_sirius_compile_options_FOUND)
  target_compile_options(${POLLUX_TARGET_NAME}
                         PRIVATE ${_sirius_compile_options})
endif()
target_compile_options(${POLLUX_TARGET_NAME} PRIVATE ${PKG_FFMPEG_CFLAGS})

# `sirius` internal compile options
target_compile_definitions(
  ${POLLUX_TARGET_NAME} PRIVATE -Dlog_module_name="${POLLUX_MODULE_PRINT_NAME}")

target_compile_definitions(${POLLUX_TARGET_NAME}
                           PRIVATE -Dexternal_log_level=${POLLUX_LOG_LEVEL})

# asan
if(POLLUX_ASAN)
  target_compile_options(
    ${POLLUX_TARGET_NAME}
    PUBLIC -fsanitize=address
    PUBLIC -fno-omit-frame-pointer
    PUBLIC -fsanitize-recover=address)
  target_link_options(${POLLUX_TARGET_NAME} PUBLIC -fsanitize=address)
endif()

# gcov
if(POLLUX_GCOV)
  target_compile_options(${POLLUX_TARGET_NAME} PUBLIC --coverage)
  target_link_options(${POLLUX_TARGET_NAME} PUBLIC --coverage)
endif()

# all warnings
if(POLLUX_WARNING_ALL)
  if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    target_compile_options(${POLLUX_TARGET_NAME} PRIVATE /W4)
  else()
    target_compile_options(${POLLUX_TARGET_NAME} PRIVATE -Wall)
  endif()
endif()

# compile warnings as errors
if(POLLUX_WARNING_ERROR)
  if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    target_compile_options(${POLLUX_TARGET_NAME} PRIVATE /WX)
  else()
    target_compile_options(${POLLUX_TARGET_NAME} PRIVATE -Werror)
  endif()
endif()

target_compile_definitions(${POLLUX_TARGET_NAME} PRIVATE POLLUX_BUILDING)

# header files
get_target_property(
  _sirius_include_dir ${sirius_NAMESPACE}::${sirius_NAMESPACE}
  INTERFACE_INCLUDE_DIRECTORIES)
target_include_directories(${POLLUX_TARGET_NAME} PRIVATE ${_sirius_include_dir})
target_include_directories(${POLLUX_TARGET_NAME}
                           PRIVATE ${PROJECT_SOURCE_DIR}/include)

# cmake interface
if(BUILD_SHARED_LIBS)
  target_link_directories(${POLLUX_TARGET_NAME} PUBLIC ${ITFC_LINK_DIR_LIST})
  target_link_libraries(${POLLUX_TARGET_NAME} PUBLIC ${EXTRA_LINK_LIBS_LIST})
else()
  target_link_directories(${POLLUX_TARGET_NAME} INTERFACE ${ITFC_LINK_DIR_LIST})
  target_link_libraries(${POLLUX_TARGET_NAME} INTERFACE ${ITFC_LINK_LIBS_LIST})
endif()

target_include_directories(${POLLUX_TARGET_NAME} INTERFACE ${ITFC_INCLUDE_DIR})
