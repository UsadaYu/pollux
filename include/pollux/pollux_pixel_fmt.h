#ifndef POLLUX_PIXEL_FMT_H
#define POLLUX_PIXEL_FMT_H

/**
 * @brief Pixel format, this enumeration inherits from the `enum AVPixelFormat`
 * of ffmpeg.
 */
typedef enum {
  pollux_pix_fmt_none = -1,

  pollux_pix_fmt_yuv420p = 0,     // planar number: 3,  YUV 4:2:0, 12bpp, YYYY-U-V...
  pollux_pix_fmt_yuyv422 = 1,     // packed,            YUV 4:2:2, 16bpp, YUYVYUYV...
  pollux_pix_fmt_rgb24 = 2,       // packed,            RGB 8:8:8, 24bpp, RGBRGB...
  pollux_pix_fmt_bgr24 = 3,       // packed,            RGB 8:8:8, 24bpp, BGRBGR...
  pollux_pix_fmt_yuv444p = 5,     // planar number: 3,  YUV 4:4:4, 24bpp, Y-U-V...
  pollux_pix_fmt_pal8 = 11,       // pal8,              8-bit color palette, refer to `AV_PIX_FMT_RGB32` of `ffmpeg`.
  pollux_pix_fmt_yuvj420p = 12,   // planar number: 3,  YUV 4:2:0, 12bpp, full scale (JPEG). According to `ffmpeg`, deprecated in favor of `AV_PIX_FMT_YUV420P` and setting color_range.
  pollux_pix_fmt_yuvj422p = 13,   // planar number: 3,  YUV 4:2:2, 16bpp, full scale (JPEG). According to `ffmpeg`, deprecated in favor of `AV_PIX_FMT_YUV422P` and setting color_range
  pollux_pix_fmt_yuvj444p = 14,   // planar number: 3,  YUV 4:4:4, 24bpp, full scale (JPEG). According to `ffmpeg`, deprecated in favor of `AV_PIX_FMT_YUV444P` and setting color_range.
  pollux_pix_fmt_bgr8 = 17,       // packed,            RGB 3:3:2, 8bpp, (msb)2B 3G 3R(lsb)
  pollux_pix_fmt_bgr4 = 18,       // packed,            RGB 1:2:1 bitstream, 4bpp, (msb)1B 2G 1R(lsb), a byte contains two pixels, the first pixel in the byte is the one composed by the 4 msb bits.
  pollux_pix_fmt_bgr4_byte = 19,  // packed,            RGB 1:2:1, 8bpp, (msb)1B 2G 1R(lsb)
  pollux_pix_fmt_rgb8 = 20,       // packed,            RGB 3:3:2, 8bpp, (msb)3R 3G 2B(lsb)
  pollux_pix_fmt_rgb4 = 21,       // packed,            RGB 1:2:1 bitstream, 4bpp, (msb)1R 2G 1B(lsb), a byte contains two pixels, the first pixel in the byte is the one composed by the 4 msb bits.
  pollux_pix_fmt_rgb4_byte = 22,  // packed,            RGB 1:2:1, 8bpp, (msb)1R 2G 1B(lsb)
  pollux_pix_fmt_nv12 = 23,       // planar number: 2,  YUV 4:2:0, 12bpp, YYYY-UV...
  pollux_pix_fmt_nv21 = 24,       // planar number: 2,  YUV 4:2:0, 12bpp, YYYY-VU...

  pollux_pix_fmt_max,
} pollux_pix_fmt_t;

#endif // POLLUX_PIXEL_FMT_H
