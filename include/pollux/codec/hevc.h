#ifndef POLLUX_CODEC_HEVC_H
#define POLLUX_CODEC_HEVC_H

/**
 * @brief Bit rate control mode for HEVC.
 */
typedef enum {
  hevc_rc_none,

  /**
   * @brief Constant Rate Factor (CRF) mode.
   */
  hevc_rc_crf,

  /**
   * @brief Constant Bitrate (CBR).
   */
  hevc_rc_cbr,

  /**
   * @brief Variable Bitrate (VBR).
   */
  hevc_rc_vbr,
} hevc_rate_t;

/**
 * @brief Tune options for HEVC.
 */
typedef enum {
  hevc_tune_none,

  /**
   * @brief Zero latency, suitable for live streaming.
   */
  hevc_tune_zerolatency,

  /**
   * @brief Optimize for fast encoding and decoding.
   */
  hevc_tune_fast_codec,
} hevc_tune_t;

/**
 * @brief Encodes parameter.
 */
typedef struct {
  /**
   * @brief Encoding speed/preset level. Higher is faster.
   *
   * @note Range: codec_range_min ~ codec_range_max.
   * If the configuration is set to 0, no changes will be made.
   */
  int speed_level;

  /**
   * @brief Rate control mode.
   *
   * @note If the configuration is set to `hevc_rc_none`, no changes will be
   * made.
   */
  hevc_rate_t rc_mode;

  /**
   * @brief Quality level for Constant Rate Factor (CRF) mode. Higher is
   * better.
   *
   * @note This parameter is used for `CRF` mode.
   * Range: codec_range_min ~ codec_range_max.
   * If the configuration is set to 0, no changes will be made.
   */
  int quality_level;

  /**
   * @brief Target bitrate in Kbps (kilobits per second).
   *
   * @note This parameter is used for `CBR/VBR` mode.
   * If the configuration is set to 0, no changes will be made.
   */
  int bitrate;

  /**
   * @brief Tuning option for the encoder.
   *
   * @note If the configuration is set to `hevc_tune_none`, no changes will be
   * made.
   */
  hevc_tune_t tune_mode;

  /**
   * @brief Group of Pictures (GOP) size. The interval between keyframes.
   *
   * @note If the configuration is set to 0, no changes will be made.
   */
  int gop_size;

  /**
   * @brief Advanced options in a key-value string format.
   *
   * @example "key1=val1:key2=val2"
   */
  const char *advanced_options;

} hevc_encode_args_t;

#endif // POLLUX_CODEC_HEVC_H
