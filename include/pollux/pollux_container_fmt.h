#ifndef POLLUX_CONTAINER_FMT_H
#define POLLUX_CONTAINER_FMT_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define POLLUX_CONT_FMT_INVALID "NONE"

/**
 * @see Command: ffmpeg -formats
 */
#define POLLUX_CONT_FORMATS \
  X(pollux_cont_fmt_none, POLLUX_CONT_FMT_INVALID) \
  X(pollux_cont_fmt_avi, "avi") \
  X(pollux_cont_fmt_gif, "gif") \
  X(pollux_cont_fmt_gif_pipe, "gif_pipe") \
  X(pollux_cont_fmt_h264, "h264") \
  X(pollux_cont_fmt_hevc, "hevc") \
  X(pollux_cont_fmt_image2, "image2") \
  X(pollux_cont_fmt_image2pipe, "image2pipe") \
  X(pollux_cont_fmt_m4v, "m4v") \
  X(pollux_cont_fmt_mjpeg, "mjpeg") \
  X(pollux_cont_fmt_mov, "mov") \
  X(pollux_cont_fmt_mp3, "mp3") \
  X(pollux_cont_fmt_mp4, "mp4") \
  X(pollux_cont_fmt_mpeg2video, "mpeg2video") \
  X(pollux_cont_fmt_mpegts, "mpegts") \
  X(pollux_cont_fmt_mpegtsraw, "mpegtsraw")

typedef enum {
#define X(name, str) name,
  POLLUX_CONT_FORMATS
#undef X
    pollux_cont_fmt_max
} pollux_cont_fmt_t;

static const char *const pollux_cont_map[] = {
#define X(name, str) str,
  POLLUX_CONT_FORMATS
#undef X
};

#ifdef __cplusplus
static_assert((sizeof(pollux_cont_map) / sizeof(pollux_cont_map[0])) ==
                (size_t)pollux_cont_fmt_max,
              "pollux_cont_map size mismatch");
#else
_Static_assert((sizeof(pollux_cont_map) / sizeof(pollux_cont_map[0])) ==
                 (size_t)pollux_cont_fmt_max,
               "pollux_cont_map size mismatch");
#endif

static inline const char *pollux_cont_enum_to_string(pollux_cont_fmt_t fmt) {
  int64_t i = (int64_t)fmt;
  if (i > (int64_t)pollux_cont_fmt_none && i < (int64_t)pollux_cont_fmt_max)
    return pollux_cont_map[i];
#ifdef __cplusplus
  return nullptr;
#else
#  if defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 202311L)
  return nullptr;
#  else
  return NULL;
#  endif
#endif
}

#ifdef __cplusplus
}
#endif

#endif // POLLUX_CONTAINER_FMT_H
