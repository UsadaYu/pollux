#ifndef POLLUX_INTERNAL_ALIGN_H
#define POLLUX_INTERNAL_ALIGN_H

#include <libavutil/cpu.h>

static inline int align_get_alignment() {
  int flags = av_get_cpu_flags();

  if ((flags & AV_CPU_FLAG_AVX512) != 0) {
    return 64;
  } else if ((flags & AV_CPU_FLAG_AVX2) != 0) {
    return 32;
  } else if ((flags & AV_CPU_FLAG_AVX) != 0) {
    return 32;
  } else if ((flags & AV_CPU_FLAG_SSE2) != 0) {
    return 16;
  } else {
    return 32;
  }
}

#endif // POLLUX_INTERNAL_ALIGN_H
