#ifndef INTERNAL_FF_ERRON_H
#define INTERNAL_FF_ERRON_H

#include <sirius/sirius_log.h>

#define internal_ff_error(ret, func)                    \
  do {                                                  \
    char e[256];                                        \
    av_strerror(ret, e, sizeof(e));                     \
    sirius_error("Ffmpeg-" #func "[%d]: %s\n", ret, e); \
  } while (0)

#define internal_ff_warn(ret, func)                    \
  do {                                                 \
    char e[256];                                       \
    av_strerror(ret, e, sizeof(e));                    \
    sirius_warn("Ffmpeg-" #func "[%d]: %s\n", ret, e); \
  } while (0)

#define internal_ff_debg(ret, func)                    \
  do {                                                 \
    char e[256];                                       \
    av_strerror(ret, e, sizeof(e));                    \
    sirius_debg("Ffmpeg-" #func "[%d]: %s\n", ret, e); \
  } while (0)

#endif  // INTERNAL_FF_ERRON_H
