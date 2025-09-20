#include "pollux/codec/hevc.h"

#include "pollux/internal/codec/codec.h"

static inline const char *map_speed_to_libx265_preset(int speed_level) {
  codec_range_check_or_set(&speed_level);

  if (speed_level >= codec_range_max)
    return "ultrafast";
  if (speed_level >= 14)
    return "superfast";
  if (speed_level >= 12)
    return "veryfast";
  if (speed_level >= 10)
    return "faster";
  if (speed_level >= 8)
    return "fast";
  if (speed_level >= 6)
    return "medium";
  if (speed_level >= 4)
    return "slow";
  if (speed_level >= 3)
    return "slower";
  if (speed_level >= 2)
    return "veryslow";

  /**
   * @brief `placebo` is not recommended for use in a production environment,
   * but it can be included for completeness.
   */
  // if (speed_level == 1) return "placebo";
  return "placebo";
}

static inline int map_quality_to_libx265_crf(int quality_level) {
  codec_range_check_or_set(&quality_level);

  /**
   * @brief `libx265` `CRF` range is 0-51, lower is better.
   * A common range is 18 (high quality) to 28 (lower quality).
   * Map `quality_level` where higher is better to the `CRF` range.
   *
   * CRF 18 <---> quality_level codec_range_max
   * CRF 33 <---> quality_level codec_range_min
   */
  int crf_value =
    33 + (quality_level - 1) * (18 - 33) / (codec_range_max - codec_range_min);

  return crf_value;
}

static void hevc_priv_set_libx265(void *cc_priv,
                                  const hevc_encode_args_t *args) {
  int ret;

  if (args->speed_level != 0) {
    const char *preset = map_speed_to_libx265_preset(args->speed_level);
    sirius_infosp("`libx265`, preset: %s\n", preset);
    ret = av_opt_set(cc_priv, "preset", preset, 0);
    if (ret)
      ffmpeg_warn(ret, "av_opt_set");
  }

  switch (args->rc_mode) {
  case hevc_rc_crf:
    if (args->quality_level != 0) {
      int crf = map_quality_to_libx265_crf(args->quality_level);
      sirius_infosp("`libx265`, crf: %d\n", crf);
      ret = av_opt_set(cc_priv, "crf", int_to_str(crf), 0);
      if (ret)
        ffmpeg_warn(ret, "av_opt_set");
    }
    break;
  case hevc_rc_cbr:
  case hevc_rc_vbr:
    if (args->bitrate > 0) {
      char bt_str[32];
      snprintf(bt_str, sizeof(bt_str), "%dK", args->bitrate);
      sirius_infosp("`libx265`, bitrate: %s\n", bt_str);
      ret = av_opt_set(cc_priv, "b", bt_str, 0);
      if (ret)
        ffmpeg_warn(ret, "av_opt_set");

      ret = av_opt_set(cc_priv, "vbv-bufsize", bt_str, 0);
      if (ret)
        ffmpeg_warn(ret, "av_opt_set");
      ret = av_opt_set(cc_priv, "vbv-maxrate", bt_str, 0);
      if (ret)
        ffmpeg_warn(ret, "av_opt_set");
    }
    break;
  default:
    break;
  }

  switch (args->tune_mode) {
  case hevc_tune_zerolatency:
    sirius_infosp("`libx265`, tune: zerolatency\n");
    ret = av_opt_set(cc_priv, "tune", "zerolatency", 0);
    if (ret)
      ffmpeg_warn(ret, "av_opt_set");
    break;
  case hevc_tune_fast_codec:
    ret = av_opt_set(cc_priv, "tune", "fastdecode", 0);
    sirius_infosp("`libx265`, tune: fastdecode\n");
    if (ret)
      ffmpeg_warn(ret, "av_opt_set");
    break;
  default:
    break;
  }

  if (args->gop_size > 0) {
    sirius_infosp("`libx265`, gop_size: %d\n", args->gop_size);
    ret = av_opt_set(cc_priv, "g", int_to_str(args->gop_size), 0);
    if (ret)
      ffmpeg_warn(ret, "av_opt_set");
  }

  if (args->advanced_options) {
    sirius_infosp("`libx265`, advanced_options: %s\n", args->advanced_options);
    ret = av_opt_set_from_string(cc_priv, args->advanced_options, nullptr, "=",
                                 ":");
    if (ret)
      ffmpeg_warn(ret, "av_opt_set_from_string");
  }
}

void hevc_priv_set(void *cc_priv, const hevc_encode_args_t *args) {
  if (!cc_priv || !args)
    return;

#ifdef codec_hevc_libx265
  hevc_priv_set_libx265(cc_priv, args);
#elif defined(codec_hevc_hevc_nvenc)
  // hevc_priv_set_hevc_nvenc(cc_priv, args);
#elif defined(codec_hevc_hevc_amf)
  // hevc_priv_set_hevc_amf(cc_priv, args);
#elif defined(codec_hevc_hevc_qsv)
  // hevc_priv_set_hevc_qsv(cc_priv, args);
#endif
}
