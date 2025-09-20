# hevc
if(POLLUX_CODEC_HEVC_LIB STREQUAL "libx265")
  target_compile_definitions(${POLLUX_TARGET_NAME} PRIVATE -Dcodec_hevc_libx265)
elseif(POLLUX_CODEC_HEVC_LIB STREQUAL "hevc_nvenc")
  target_compile_definitions(${POLLUX_TARGET_NAME}
                             PRIVATE -Dcodec_hevc_hevc_nvenc)
elseif(POLLUX_CODEC_HEVC_LIB STREQUAL "hevc_amf")
  target_compile_definitions(${POLLUX_TARGET_NAME}
                             PRIVATE -Dcodec_hevc_hevc_amf)
elseif(POLLUX_CODEC_HEVC_LIB STREQUAL "hevc_qsv")
  target_compile_definitions(${POLLUX_TARGET_NAME}
                             PRIVATE -Dcodec_hevc_hevc_qsv)
else()
  message(FATAL_ERROR "Unsupported `HEVC` codec lib: ${POLLUX_CODEC_HEVC_LIB}")
endif()
message(STATUS "`HEVC` codec lib: ${POLLUX_CODEC_HEVC_LIB}")

# av1
if(POLLUX_CODEC_AV1_LIB STREQUAL "libsvtav1")
  target_compile_definitions(${POLLUX_TARGET_NAME}
                             PRIVATE -Dcodec_av1_libsvtav1)
elseif(POLLUX_CODEC_AV1_LIB STREQUAL "libaom-av1")
  target_compile_definitions(${POLLUX_TARGET_NAME}
                             PRIVATE -Dcodec_av1_libaom_av1)
elseif(POLLUX_CODEC_AV1_LIB STREQUAL "av1_nvenc")
  target_compile_definitions(${POLLUX_TARGET_NAME}
                             PRIVATE -Dcodec_av1_av1_nvenc)
elseif(POLLUX_CODEC_AV1_LIB STREQUAL "av1_amf")
  target_compile_definitions(${POLLUX_TARGET_NAME} PRIVATE -Dcodec_av1_av1_amf)
elseif(POLLUX_CODEC_AV1_LIB STREQUAL "av1_qsv")
  target_compile_definitions(${POLLUX_TARGET_NAME} PRIVATE -Dcodec_av1_av1_qsv)
elseif(POLLUX_CODEC_AV1_LIB STREQUAL "librav1e")
  target_compile_definitions(${POLLUX_TARGET_NAME} PRIVATE -Dcodec_av1_librav1e)
else()
  message(FATAL_ERROR "Unsupported `AV1` codec lib: ${POLLUX_CODEC_AV1_LIB}")
endif()
message(STATUS "`AV1` codec lib: ${POLLUX_CODEC_AV1_LIB}")
