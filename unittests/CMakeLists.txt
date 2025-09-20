add_subdirectory(cmake)

set(_generated_file_pre "_test")

set(_artifact_path "${CMAKE_CURRENT_BINARY_DIR}/artifact")
set(_artifact_inc_path "${_artifact_path}/include")
set(_artifact_bin_path "${_artifact_path}/bin")

set(_link_dir_list "")
set(_link_libs_list "")
set(_include_dir_list "")

file(GLOB _header_list "${PROJECT_SOURCE_DIR}/include/pollux/*.h")
file(COPY ${_header_list} DESTINATION "${_artifact_inc_path}/pollux")

file(GLOB _header_codec_list "${PROJECT_SOURCE_DIR}/include/pollux/codec/*.h")
file(COPY ${_header_codec_list}
     DESTINATION "${_artifact_inc_path}/pollux/codec")

file(GLOB _test_video "${CMAKE_CURRENT_SOURCE_DIR}/input*.*")
file(COPY ${_test_video} DESTINATION ${_artifact_bin_path})

set(_src_dir "${CMAKE_CURRENT_SOURCE_DIR}/src")
file(GLOB _src_list "${_src_dir}/*.cpp" "${_src_dir}/*.c")

get_target_property(_sirius_link_dir ${sirius_NAMESPACE}::${sirius_NAMESPACE}
                    INTERFACE_LINK_DIRECTORIES)
get_target_property(
  _sirius_link_libraries ${sirius_NAMESPACE}::${sirius_NAMESPACE}
  INTERFACE_LINK_LIBRARIES)
get_target_property(
  _sirius_include_dir ${sirius_NAMESPACE}::${sirius_NAMESPACE}
  INTERFACE_INCLUDE_DIRECTORIES)
get_target_property(
  _sirius_compile_options ${sirius_NAMESPACE}::${sirius_NAMESPACE}
  INTERFACE_COMPILE_OPTIONS)

list(APPEND _link_dir_list ${TARGET_LIB_DIR})
list(APPEND _link_dir_list ${_sirius_link_dir})
list(APPEND _link_dir_list ${PKG_FFMPEG_LIBRARY_DIRS})
list(APPEND _link_dir_list ${POLLUX_TEST_EXTRA_LINK_DIR})
list(APPEND _link_libs_list ${POLLUX_TARGET_NAME})
list(APPEND _link_libs_list ${sirius_NAMESPACE}::${sirius_NAMESPACE})
list(APPEND _link_libs_list ${_sirius_link_libraries})
list(APPEND _link_libs_list ${PKG_FFMPEG_LIBRARIES})
list(APPEND _link_libs_list ${POLLUX_TEST_EXTRA_LINK_LIBRARIES})
list(APPEND _include_dir_list ${_artifact_inc_path})
list(APPEND _include_dir_list ${_sirius_include_dir})

foreach(file ${_src_list})
  # get_filename_component(file_name ${file} NAME_WE)
  get_filename_component(full_name ${file} NAME)
  string(REGEX REPLACE "\\.[^.]+$" "" file_name ${full_name})
  set(_target_name "${POLLUX_TARGET_NAME}_${file_name}")

  add_executable(${_target_name} ${file})
  set_target_properties(${_target_name} PROPERTIES RUNTIME_OUTPUT_DIRECTORY
                                                   ${_artifact_bin_path})

  target_link_directories(${_target_name} PRIVATE ${_link_dir_list})
  target_link_libraries(${_target_name} PRIVATE ${_link_libs_list})
  target_include_directories(${_target_name} PRIVATE ${_include_dir_list})

  if(NOT CMAKE_SYSTEM_NAME STREQUAL "Windows")
    if(${POLLUX_TEST_PIE_ENABLE})
      target_compile_options(${_target_name} PRIVATE -fPIE)
      target_link_options(${_target_name} PRIVATE -pie)
    else()
      target_link_options(${_target_name} PRIVATE -no-pie)
    endif()
  else()
    if(BUILD_SHARED_LIBS)
      add_custom_command(
        TARGET ${_target_name}
        POST_BUILD
        COMMAND
          ${CMAKE_COMMAND} -E copy_if_different
          $<TARGET_FILE:${POLLUX_TARGET_NAME}>
          $<TARGET_FILE_DIR:${_target_name}>
        COMMENT "Copy ${POLLUX_TARGET_NAME} dll next to ${_target_name}")
    endif()
  endif()

  if(${CMAKE_CXX_COMPILER_ID} STREQUAL "Clang" OR ${CMAKE_CXX_COMPILER_ID}
                                                  STREQUAL "GNU")
    target_compile_options(${_target_name} PRIVATE -Wall)
  elseif(${CMAKE_CXX_COMPILER_ID} STREQUAL "MSVC")
    target_compile_options(${_target_name} PRIVATE /W4)
  endif()

  target_compile_definitions(
    ${_target_name}
    PRIVATE -Dlog_module_name="${file_name}"
    PRIVATE -Dexternal_log_level=${POLLUX_TEST_EXTERNAL_LOG_LEVEL}
    PRIVATE -Dtest_generated_pre="${_generated_file_pre}")

  if(POLLUX_TEST_LOG_PATH)
    target_compile_definitions(
      ${_target_name}
      PRIVATE -Dtest_log_path_overload
      PRIVATE -Dtest_log_path="${POLLUX_TEST_LOG_PATH}")
  endif()

  add_test(
    NAME ${_target_name}
    COMMAND ./${_target_name}
    WORKING_DIRECTORY ${_artifact_bin_path})
endforeach()

if(IS_ABSOLUTE ${POLLUX_TEST_LOG_PATH})
  set(_test_log_to_delete ${POLLUX_TEST_LOG_PATH})
else()
  set(_test_log_to_delete "${_artifact_bin_path}/${POLLUX_TEST_LOG_PATH}")
endif()

set(GLOB_TEST_GENERATED_TO_DELETE
    "${_artifact_bin_path}/${_generated_file_pre}*")
set(GLOB_TEST_LOG_TO_DELETE "${_test_log_to_delete}")
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/cmake/clean_script.cmake.in"
               "${CMAKE_CURRENT_BINARY_DIR}/custom_clean.cmake" @ONLY)
add_custom_target(
  custom_clean
  COMMAND ${CMAKE_COMMAND} -P "${CMAKE_CURRENT_BINARY_DIR}/custom_clean.cmake"
  COMMENT "Deleting test-generated files"
  VERBATIM)
