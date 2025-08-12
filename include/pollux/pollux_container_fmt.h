#ifndef POLLUX_CONTAINER_FMT_H
#define POLLUX_CONTAINER_FMT_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
  pollux_cont_fmt_none,

  pollux_cont_fmt_avi,                // AVI (Audio Video Interleaved).
  pollux_cont_fmt_gif,                // CompuServe Graphics Interchange Format (GIF).
  pollux_cont_fmt_gif_pipe,           // Piped gif sequence.
  pollux_cont_fmt_h264,               // Raw H.264 video.
  pollux_cont_fmt_hevc,               // Raw HEVC video.
  pollux_cont_fmt_image2,             // `image2` sequence.
  pollux_cont_fmt_image2pipe,         // Piped image2 sequence.
  pollux_cont_fmt_m4v,                // Raw MPEG-4 video.
  pollux_cont_fmt_mjpeg,              // Raw MJPEG video.
  pollux_cont_fmt_mov,                // QuickTime / MOV.
  pollux_cont_fmt_mp3,                // MP3 (MPEG audio layer 3).
  pollux_cont_fmt_mp4,                // MP4 (MPEG-4 Part 14).
  pollux_cont_fmt_mpeg2video,         // Raw MPEG-2 video.
  pollux_cont_fmt_mpegts,             // MPEG-TS (MPEG-2 Transport Stream).
  pollux_cont_fmt_mpegtsraw,          // Raw MPEG-TS (MPEG-2 Transport Stream).

  pollux_cont_fmt_max,
} pollux_cont_fmt_t;

static const char* const
    pollux_cont_map[pollux_cont_fmt_max] = {
        [pollux_cont_fmt_none] = "",
        [pollux_cont_fmt_avi] = "avi",
        [pollux_cont_fmt_gif] = "gif",
        [pollux_cont_fmt_gif_pipe] = "gif_pipe",
        [pollux_cont_fmt_h264] = "h264",
        [pollux_cont_fmt_hevc] = "hevc",
        [pollux_cont_fmt_image2] = "image2",
        [pollux_cont_fmt_image2pipe] = "image2pipe",
        [pollux_cont_fmt_m4v] = "m4v",
        [pollux_cont_fmt_mjpeg] = "mjpeg",
        [pollux_cont_fmt_mov] = "mov",
        [pollux_cont_fmt_mp3] = "mp3",
        [pollux_cont_fmt_mp4] = "mp4",
        [pollux_cont_fmt_mpeg2video] = "mpeg2video",
        [pollux_cont_fmt_mpegts] = "mpegts",
        [pollux_cont_fmt_mpegtsraw] = "mpegtsraw",
};

static inline const char* pollux_cont_to_string(
    pollux_cont_fmt_t fmt) {
  if (fmt > pollux_cont_fmt_none &&
      fmt < pollux_cont_fmt_max) {
    return pollux_cont_map[fmt];
  }
  return nullptr;
}

#ifdef __cplusplus
}
#endif

#endif  // POLLUX_CONTAINER_FMT_H
