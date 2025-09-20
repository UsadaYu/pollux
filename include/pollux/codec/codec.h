#ifndef POLLUX_CODEC_CODEC_H
#define POLLUX_CODEC_CODEC_H

#include "pollux/codec/av1.h"
#include "pollux/codec/hevc.h"

#ifndef codec_range_min
#  define codec_range_min (1)
#else
#  warning "`codec_range_min` is defined elsewhere"
#endif

#ifndef codec_range_max
#  define codec_range_max (16)
#else
#  warning "`codec_range_max` is defined elsewhere"
#endif

#endif // POLLUX_CODEC_CODEC_H
