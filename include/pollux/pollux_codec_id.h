#ifndef POLLUX_CODEC_ID_H
#define POLLUX_CODEC_ID_H

/**
 * @brief Encoding format id, this enumeration inherits from the
 * `enum AVCodecID` of ffmpeg.
 */
typedef enum pollux_codec_id_t {
  pollux_codec_id_none = 0,
  pollux_codec_id_mjpeg = 7,
  pollux_codec_id_h264 = 27,
  pollux_codec_id_png = 61,
  pollux_codec_id_gif = 97,
  pollux_codec_id_hevc = 173,
  pollux_codec_id_av1 = 225,

  pollux_codec_id_max,
} pollux_codec_id_t;

#endif // POLLUX_CODEC_ID_H
