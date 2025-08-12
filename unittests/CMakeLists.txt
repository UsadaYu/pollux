add_subdirectory(cmake)

set(_artifact_path "${CMAKE_CURRENT_BINARY_DIR}/artifact")
set(_artifact_inc_path "${_artifact_path}/include")
set(_artifact_bin_path "${_artifact_path}/bin")

file(GLOB _header_list "${PROJECT_SOURCE_DIR}/include/pollux/*.h")
file(COPY ${_header_list} DESTINATION "${_artifact_inc_path}/pollux")

file(GLOB _test_video "${CMAKE_CURRENT_SOURCE_DIR}/input*.*")
file(COPY ${_test_video} DESTINATION ${_artifact_bin_path})

set(_src_dir "${CMAKE_CURRENT_SOURCE_DIR}/src")
file(GLOB _src_list "${_src_dir}/*.cpp" "${_src_dir}/*.c")

get_target_property(
  _sirius_link_dir ${sirius_NAMESPACE}::${sirius_NAMESPACE}
  INTERFACE_LINK_DIRECTORIES
)
get_target_property(
  _sirius_link_libraries ${sirius_NAMESPACE}::${sirius_NAMESPACE}
  INTERFACE_LINK_LIBRARIES
)
get_target_property(
  _sirius_include_dir ${sirius_NAMESPACE}::${sirius_NAMESPACE}
  INTERFACE_INCLUDE_DIRECTORIES
)

list(
  APPEND _link_dir_list
  ${TARGET_LIB_DIR}
  ${_sirius_link_dir}
  ${PKG_FFMPEG_LIBRARY_DIRS}
  ${POLLUX_TEST_EXTRA_LINK_DIR}
)

list(
  APPEND _link_libs_list
  ${POLLUX_TARGET_NAME}
  ${sirius_NAMESPACE}::${sirius_NAMESPACE}
  ${_sirius_link_libraries}
  ${PKG_FFMPEG_LIBRARIES}
  ${POLLUX_TEST_EXTRA_LINK_LIBRARIES}
)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  #[[
    ffmpeg's pkgconfig file uses `-pthread` instead of
    `-lpthread` for the link option to the thread library,
    therefore, cmake's `pkg_check_modules` command cannot
    recognize it.
    so the option to link the thread library is added here
    manually

    ffmpeg version: 7.1
    cmake version: 3.30.2
    date: 2024-10-12
  ]]
  list(APPEND _link_libs_list "pthread")
endif()

set(_generated_file_pre "_test")

foreach(file ${_src_list})
  # get_filename_component(file_name ${file} NAME_WE)
  get_filename_component(full_name ${file} NAME)
  string(REGEX REPLACE "\\.[^.]+$" "" file_name ${full_name})
  set(_target_name "${POLLUX_TARGET_NAME}_${file_name}")

  add_executable(${_target_name} ${file})
  set_target_properties(
    ${_target_name}
    PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${_artifact_bin_path}
  )

  target_include_directories(
    ${_target_name}
    PRIVATE ${_artifact_inc_path}
    PRIVATE ${_sirius_include_dir}
  )

  target_link_directories(${_target_name} PRIVATE ${_link_dir_list})
  target_link_libraries(${_target_name} PRIVATE ${_link_libs_list})

  if(NOT CMAKE_SYSTEM_NAME STREQUAL "Windows")
    if(${POLLUX_TEST_PIE_ENABLE})
      target_compile_options(${_target_name} PRIVATE -fPIE)
      target_link_options(${_target_name} PRIVATE -pie)
    else()
      target_link_options(${_target_name} PRIVATE -no-pie)
    endif()
  endif()

  if(${CMAKE_CXX_COMPILER_ID} STREQUAL "Clang" OR
    ${CMAKE_CXX_COMPILER_ID} STREQUAL "GNU")
    target_compile_options(${_target_name} PRIVATE -Wall)
  elseif(${CMAKE_CXX_COMPILER_ID} STREQUAL "MSVC")
    target_compile_options(${_target_name} PRIVATE /W4)
  endif()

  target_compile_definitions(
    ${_target_name}
    PRIVATE -Dlog_module_name="${file_name}"
    PRIVATE -Dexternal_log_level=${POLLUX_TEST_EXTERNAL_LOG_LEVEL}
    PRIVATE -Dtest_generated_pre="${_generated_file_pre}"
  )

  if(POLLUX_TEST_LOG_PATH)
    target_compile_definitions(
      ${_target_name}
      PRIVATE -Dtest_log_path_overload
      PRIVATE -Dtest_log_path="${POLLUX_TEST_LOG_PATH}"
    )
  endif()

  add_test(
    NAME ${_target_name}
    COMMAND ./${_target_name}
    WORKING_DIRECTORY ${_artifact_bin_path}
  )
endforeach()

if(IS_ABSOLUTE ${POLLUX_TEST_LOG_PATH})
  set(_test_log_to_delete ${POLLUX_TEST_LOG_PATH})
else()
  set(_test_log_to_delete "${_artifact_bin_path}/${POLLUX_TEST_LOG_PATH}")
endif()

set(GLOB_TEST_GENERATED_TO_DELETE "${_artifact_bin_path}/${_generated_file_pre}*")
set(GLOB_TEST_LOG_TO_DELETE "${_test_log_to_delete}")
configure_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/cmake/clean_script.cmake.in"
  "${CMAKE_CURRENT_BINARY_DIR}/custom_clean.cmake"
  @ONLY
)
add_custom_target(
  custom_clean
  COMMAND ${CMAKE_COMMAND} -P "${CMAKE_CURRENT_BINARY_DIR}/custom_clean.cmake"
  COMMENT "Deleting test-generated files"
  VERBATIM
)
