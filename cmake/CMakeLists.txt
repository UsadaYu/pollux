# requirement
file(
  READ "${CMAKE_CURRENT_SOURCE_DIR}/requirement.txt"
  _require_content
)
macro(
  version_read
  VERSION_VARIABLE REQUIRE_CONTENT REGEX_PREFIX
)
  string(
    REGEX MATCH "${REGEX_PREFIX}_VERSION=([0-9.]+)"
    _version_match "${REQUIRE_CONTENT}"
  )
  if(_version_match)
    set(${VERSION_VARIABLE} "${CMAKE_MATCH_1}")
  endif()
endmacro()
version_read(_required_gnuc_version "${_require_content}" "GNUC")
version_read(_required_clang_version "${_require_content}" "CLANG")
version_read(_required_msvc_version "${_require_content}" "MSVC")
unset(_require_content)

macro(
  version_check
  REQUIRE_CONTENT
)
  if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS ${REQUIRE_CONTENT})
    message(WARNING
      "${CMAKE_CXX_COMPILER_ID} version: "
      "[current: ${CMAKE_CXX_COMPILER_VERSION}] "
      "[required: ${REQUIRE_CONTENT}]\n"
      "please edit cmake cache or rebuild")
  endif()
endmacro()
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  version_check("${_required_gnuc_version}")
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
  version_check("${_required_clang_version}")
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
  version_check("${_required_msvc_version}")
else()
  message(FATAL_ERROR
    "${CMAKE_CXX_COMPILER_ID} is not supported")
endif()
unset(_required_gnuc_version)
unset(_required_clang_version)
unset(_required_msvc_version)

# install directory
set(_target_lib_dirr "lib")
set(_target_pkg_dirr "${_target_lib_dirr}/pkgconfig")
set(_target_package_dirr "${_target_lib_dirr}/cmake/${POLLUX_TARGET_NAME}")
set(_target_lib_dir "${CMAKE_INSTALL_PREFIX}/${_target_lib_dirr}")
set(_target_pkg_dir "${CMAKE_INSTALL_PREFIX}/${_target_pkg_dirr}")
set(_target_package_dir "${CMAKE_INSTALL_PREFIX}/${_target_package_dirr}")

# args
set(EXTRA_LINK_DIR_LIST "")
set(EXTRA_LINK_LIBS_LIST "")
set(ITFC_LINK_DIR_LIST "")
set(ITFC_LINK_LIBS_LIST "")
set(ITFC_INCLUDE_DIR "")

get_target_property(
  _sirius_link_dir ${sirius_NAMESPACE}::${sirius_NAMESPACE}
  INTERFACE_LINK_DIRECTORIES
)
get_target_property(
  _sirius_link_libraries ${sirius_NAMESPACE}::${sirius_NAMESPACE}
  INTERFACE_LINK_LIBRARIES
)

list(
  APPEND EXTRA_LINK_DIR_LIST
  ${_sirius_link_dir}
  ${PKG_FFMPEG_LIBRARY_DIRS}
  ${POLLUX_EXTRA_LINK_DIR}
)
list(
  APPEND EXTRA_LINK_LIBS_LIST
  ${sirius_NAMESPACE}::${sirius_NAMESPACE}
  ${_sirius_link_libraries}
  ${PKG_FFMPEG_LIBRARIES}
  ${POLLUX_EXTRA_LINK_LIBRARIES}
)
list(
  APPEND ITFC_LINK_DIR_LIST
  ${_target_lib_dir}
  ${EXTRA_LINK_DIR_LIST}
)
list(
  APPEND ITFC_LINK_LIBS_LIST
  ${POLLUX_TARGET_NAME}
  ${EXTRA_LINK_LIBS_LIST}
)
list(
  APPEND ITFC_INCLUDE_DIR
  "${CMAKE_INSTALL_PREFIX}/include"
)

include(${CMAKE_CURRENT_SOURCE_DIR}/external_option.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/internal_option.cmake)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

# pkg interface
set(_pkg_interface_link_dir "")
set(_pkg_interface_link_libraries "")
set(_pkg_interface_private_link_dir "")
set(_pkg_interface_private_link_libraries "")

if(BUILD_SHARED_LIBS)
  list(APPEND _pkg_interface_link_dir "-L${_target_lib_dir}")

  foreach(dir IN LISTS EXTRA_LINK_DIR_LIST)
    if(NOT dir)
      continue()
    endif()
    list(APPEND _pkg_interface_private_link_dir "-L${dir}")
  endforeach()
  list(JOIN _pkg_interface_private_link_dir " " _pkg_interface_private_link_dir)
else()
  foreach(dir IN LISTS ITFC_LINK_DIR_LIST)
    if(NOT dir)
      continue()
    endif()
    list(APPEND _pkg_interface_link_dir "-L${dir}")
  endforeach()
  list(JOIN _pkg_interface_link_dir " " _pkg_interface_link_dir)
endif()
unset(dir)

if(BUILD_SHARED_LIBS)
  list(APPEND _pkg_interface_link_libraries "-l${POLLUX_TARGET_NAME}")

  foreach(lib IN LISTS EXTRA_LINK_LIBS_LIST)
    if(NOT lib)
      continue()
    elseif(lib MATCHES "::")
      string(REGEX REPLACE ".*::" "" clean "${lib}")
    else()
      set(clean "${lib}")
    endif()

    list(APPEND _pkg_interface_private_link_libraries "-l${clean}")
  endforeach()
  list(JOIN _pkg_interface_private_link_libraries " " _pkg_interface_private_link_libraries)
else()
  foreach(lib IN LISTS ITFC_LINK_LIBS_LIST)
    if(NOT lib)
      continue()
    elseif(lib MATCHES "::")
      string(REGEX REPLACE ".*::" "" clean "${lib}")
    else()
      set(clean "${lib}")
    endif()

    list(APPEND _pkg_interface_link_libraries "-l${clean}")
  endforeach()
  list(JOIN _pkg_interface_link_libraries " " _pkg_interface_link_libraries)
endif()
unset(clean)
unset(lib)

set(
  _pkg_file
  "${PROJECT_BINARY_DIR}/${_target_pkg_dirr}/${POLLUX_TARGET_NAME}.pc"
)
configure_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/pollux.pc.in"
  ${_pkg_file}
  @ONLY
)
install(
  FILES ${_pkg_file}
  DESTINATION ${_target_pkg_dir}
)

# cmake interface
install(
  TARGETS ${POLLUX_TARGET_NAME}
  EXPORT "${POLLUX_TARGET_NAME}Targets"
  DESTINATION ${_target_lib_dir}
)
set_target_properties(
  ${POLLUX_TARGET_NAME} PROPERTIES
  VERSION ${POLLUX_VERSION}
)

set(
  _version_file
  "${PROJECT_BINARY_DIR}/${_target_package_dirr}/${POLLUX_TARGET_NAME}ConfigVersion.cmake"
)
set(
  _config_file
  "${PROJECT_BINARY_DIR}/${_target_package_dirr}/${POLLUX_TARGET_NAME}Config.cmake"
)
configure_package_config_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/Config.cmake.in
  ${_config_file}
  INSTALL_DESTINATION ${_target_package_dir}
)
write_basic_package_version_file(
  ${_version_file}
  VERSION ${POLLUX_VERSION}
  COMPATIBILITY AnyNewerVersion
)
install(
  FILES ${_version_file} ${_config_file}
  DESTINATION ${_target_package_dir}
)
install(
  EXPORT "${POLLUX_TARGET_NAME}Targets"
  FILE "${POLLUX_TARGET_NAME}Targets.cmake"
  NAMESPACE ${POLLUX_TARGET_NAME}::
  DESTINATION ${_target_package_dir}
)

# header file install
install(
  DIRECTORY ${PROJECT_SOURCE_DIR}/include/pollux
  DESTINATION ${CMAKE_INSTALL_PREFIX}/include
  FILES_MATCHING PATTERN "*.h"
  PATTERN "internal" EXCLUDE
)
