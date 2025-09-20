#include "pollux/internal/codec/codec.h"

void codec_priv_set(void *cc_priv, pollux_codec_id_t codec_id,
                    const void *args) {
  switch (codec_id) {
  case pollux_codec_id_h264:
    sirius_warnsp("Not yet: h264\n");
    break;
  case pollux_codec_id_hevc:
    hevc_priv_set(cc_priv, args);
    break;
  case pollux_codec_id_av1:
    av1_priv_set(cc_priv, args);
    break;
  default:
    sirius_warnsp("Invalid codec id: %lld\n", (int64_t)codec_id);
    return;
  }
}
