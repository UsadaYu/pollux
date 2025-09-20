#include "pollux/codec/av1.h"

#include "pollux/internal/codec/codec.h"

static inline int map_speed_to_libsvtav1_preset(int speed_level) {
  codec_range_check_or_set(&speed_level);

  /**
   * @brief `libsvtav1` presets range from 0 (slowest) to 13 (fastest).
   */
  if (speed_level >= 16)
    return 13;
  if (speed_level >= 15)
    return 12;
  if (speed_level >= 14)
    return 11;
  if (speed_level >= 13)
    return 10;
  if (speed_level >= 11)
    return 9;
  if (speed_level >= 9)
    return 8; // Default preset for `libsvtav1` is 8.
  if (speed_level >= 7)
    return 7;
  if (speed_level >= 6)
    return 6;
  if (speed_level >= 5)
    return 5;
  if (speed_level >= 4)
    return 4;
  if (speed_level >= 3)
    return 3;
  if (speed_level >= 2)
    return 2;

  return 1;
}

static inline int map_quality_to_libsvtav1_crf(int quality_level) {
  codec_range_check_or_set(&quality_level);

  /**
   * @brief `libsvtav1` `CRF` range is 0-63, lower is better.
   * A common range is 20 (high quality) to 40 (lower quality).
   * Map `quality_level` where higher is better to the `CRF` range.
   *
   * CRF 20 <---> quality_level codec_range_max
   * CRF 40 <---> quality_level codec_range_min
   */
  const int crf_high_quality = 20, crf_low_quality = 40;

  int crf_value = crf_low_quality +
    (quality_level - codec_range_min) * (crf_high_quality - crf_low_quality) /
      (codec_range_max - codec_range_min);

  return crf_value;
}

static void av1_priv_set_libsvtav1(void *cc_priv,
                                   const av1_encode_args_t *args) {
  int ret;

  if (args->speed_level != 0) {
    int preset = map_speed_to_libsvtav1_preset(args->speed_level);
    sirius_infosp("`libsvtav1`, preset: %d\n", preset);
    ret = av_opt_set(cc_priv, "preset", int_to_str(preset), 0);
    if (ret)
      ffmpeg_warn(ret, "av_opt_set preset");
  }

  switch (args->rc_mode) {
  case av1_rc_cq:
    if (args->quality_level != 0) {
      int crf = map_quality_to_libsvtav1_crf(args->quality_level);
      sirius_infosp("`libsvtav1`, crf: %d\n", crf);
      ret = av_opt_set(cc_priv, "crf", int_to_str(crf), 0);
      if (ret)
        ffmpeg_warn(ret, "av_opt_set crf");
    }
    break;
  case av1_rc_cbr:
  case av1_rc_vbr:
    if (args->bitrate > 0) {
      char bt_str[32];
      snprintf(bt_str, sizeof(bt_str), "%dK", args->bitrate);
      sirius_infosp("`libsvtav1`, bitrate: %s\n", bt_str);
      /**
       * @brief For libsvtav1, bitrate is typically set on the context, but
       * `b` option should also work.
       */
      ret = av_opt_set(cc_priv, "b", bt_str, 0);
      if (ret < 0)
        ffmpeg_warn(ret, "av_opt_set bitrate");
    }
    break;
  default:
    break;
  }

  switch (args->tune_mode) {
  case av1_tune_visual_quality:
    sirius_infosp("`libsvtav1`, tune: visual_quality (0)\n");
    ret = av_opt_set(cc_priv, "tune", "0", 0);
    if (ret < 0)
      ffmpeg_warn(ret, "av_opt_set tune");
    break;
  case av1_tune_psnr:
    sirius_infosp("`libsvtav1`, tune: psnr (1)\n");
    ret = av_opt_set(cc_priv, "tune", "1", 0);
    if (ret < 0)
      ffmpeg_warn(ret, "av_opt_set tune");
    break;
  default:
    break;
  }

  if (args->gop_size > 0) {
    sirius_infosp("`libsvtav1`, gop_size: %d\n", args->gop_size);
    ret = av_opt_set(cc_priv, "g", int_to_str(args->gop_size), 0);
    if (ret < 0)
      ffmpeg_warn(ret, "av_opt_set gop_size");
  }

  if (args->advanced_options) {
    sirius_infosp("`libsvtav1`, advanced_options: %s\n",
                  args->advanced_options);
    ret =
      av_opt_set_from_string(cc_priv, args->advanced_options, NULL, "=", ":");
    if (ret < 0)
      ffmpeg_warn(ret, "av_opt_set_from_string");
  }
}

void av1_priv_set(void *cc_priv, const av1_encode_args_t *args) {
  if (!cc_priv || !args)
    return;

#ifdef codec_av1_libsvtav1
  av1_priv_set_libsvtav1(cc_priv, args);
#elif defined(codec_av1_libaom_av1)
  // av1_priv_set_libaom_av1(cc_priv, args);
#elif defined(codec_av1_av1_nvenc)
  // av1_priv_set_av1_nvenc(cc_priv, args);
#elif defined(codec_av1_av1_amf)
  // av1_priv_set_av1_amf(cc_priv, args);
#elif defined(codec_av1_av1_qsv)
  // av1_priv_set_av1_qsv(cc_priv, args);
#elif defined(codec_av1_librav1e)
  // av1_priv_set_av1_librav1e(cc_priv, args);
#endif
}
