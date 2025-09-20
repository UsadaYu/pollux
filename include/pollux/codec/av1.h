#ifndef POLLUX_CODEC_AV1_H
#define POLLUX_CODEC_AV1_H

/**
 * @brief Bit rate control mode for AV1.
 */
typedef enum {
  av1_rc_none,

  /**
   * @brief Constant Quality (CQ) mode, also known as CRF in some encoders.
   * This is the recommended mode for high-quality encoding.
   */
  av1_rc_cq,

  /**
   * @brief Constant Bitrate (CBR).
   */
  av1_rc_cbr,

  /**
   * @brief Variable Bitrate (VBR).
   */
  av1_rc_vbr,
} av1_rate_t;

/**
 * @brief Tune options for AV1.
 */
typedef enum {
  av1_tune_none,

  /**
   * @brief Optimize for subjective visual quality. This is the default.
   */
  av1_tune_visual_quality,

  /**
   * @brief Optimize for objective metrics like PSNR/SSIM.
   */
  av1_tune_psnr,
} av1_tune_t;

/**
 * @brief AV1 encodes parameter.
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
   * @note If the configuration is set to `av1_rc_none`, no changes will be
   * made.
   */
  av1_rate_t rc_mode;

  /**
   * @brief Quality level for Constant Quality (CQ/CRF) mode. Higher is better.
   *
   * @note This parameter is used for `CQ` mode.
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
   * @note If the configuration is set to `av1_tune_none`, no changes will be
   * made.
   */
  av1_tune_t tune_mode;

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

} av1_encode_args_t;

#endif // POLLUX_CODEC_AV1_H
