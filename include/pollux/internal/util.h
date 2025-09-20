#ifndef INTERNAL_UTIL_H
#define INTERNAL_UTIL_H

#include <sirius/sirius_log.h>
#include <sirius/sirius_math.h>

#include "pollux/internal/decls.h"

#define ffmpeg_error(ret, msg) \
  do { \
    char e[256]; \
    av_strerror(ret, e, sizeof(e)); \
    sirius_error("\n`ffmpeg` -> " msg "\nError code: %d -> %s\n\n", ret, e); \
  } while (0)

#define ffmpeg_warn(ret, msg) \
  do { \
    char e[256]; \
    av_strerror(ret, e, sizeof(e)); \
    sirius_warn("\n`ffmpeg` -> " msg "\nError code: %d -> %s\n\n", ret, e); \
  } while (0)

#define ffmpeg_debg(ret, msg) \
  do { \
    char e[256]; \
    av_strerror(ret, e, sizeof(e)); \
    sirius_debg("\n`ffmpeg` -> " msg "\nError code: %d -> %s\n\n", ret, e); \
  } while (0)

static inline void range_set(int min, int max, int *range) {
  *range = sirius_max(min, *range);
  *range = sirius_min(max, *range);
}

static inline char *int_to_str(int val) {
  static char buffer[16];
  snprintf(buffer, sizeof(buffer), "%d", val);
  return buffer;
}

#endif // INTERNAL_UTIL_H
