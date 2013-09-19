// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CODEC_VIDEO_DECODER_VP8_H_
#define REMOTING_CODEC_VIDEO_DECODER_VP8_H_

#include "base/compiler_specific.h"
#include "remoting/codec/video_decoder.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_geometry.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_region.h"

typedef struct vpx_codec_ctx vpx_codec_ctx_t;
typedef struct vpx_image vpx_image_t;

namespace remoting {

class VideoDecoderVp8 : public VideoDecoder {
 public:
  VideoDecoderVp8();
  virtual ~VideoDecoderVp8();

  // VideoDecoder implementations.
  virtual bool IsReadyForData() OVERRIDE;
  virtual void Initialize(const webrtc::DesktopSize& screen_size) OVERRIDE;
  virtual DecodeResult DecodePacket(const VideoPacket* packet) OVERRIDE;
  virtual VideoPacketFormat::Encoding Encoding() OVERRIDE;
  virtual void Invalidate(const webrtc::DesktopSize& view_size,
                          const webrtc::DesktopRegion& region) OVERRIDE;
  virtual void RenderFrame(const webrtc::DesktopSize& view_size,
                           const webrtc::DesktopRect& clip_area,
                           uint8* image_buffer,
                           int image_stride,
                           webrtc::DesktopRegion* output_region) OVERRIDE;
  virtual const webrtc::DesktopRegion* GetImageShape() OVERRIDE;

 private:
  enum State {
    kUninitialized,
    kReady,
    kError,
  };

  // Fills the rectangle |rect| with the given ARGB color |color| in |buffer|.
  void FillRect(uint8* buffer, int stride,
                const webrtc::DesktopRect& rect,
                uint32 color);

  // Calculates the difference between the desktop shape regions in two
  // consecutive frames and updates |updated_region_| and |transparent_region_|
  // accordingly.
  void UpdateImageShapeRegion(webrtc::DesktopRegion* new_desktop_shape);

  // The internal state of the decoder.
  State state_;

  vpx_codec_ctx_t* codec_;

  // Pointer to the last decoded image.
  vpx_image_t* last_image_;

  // The region updated that hasn't been copied to the screen yet.
  webrtc::DesktopRegion updated_region_;

  // Output dimensions.
  webrtc::DesktopSize screen_size_;

  // The region occupied by the top level windows.
  webrtc::DesktopRegion desktop_shape_;

  // The region that should be make transparent.
  webrtc::DesktopRegion transparent_region_;

  DISALLOW_COPY_AND_ASSIGN(VideoDecoderVp8);
};

}  // namespace remoting

#endif  // REMOTING_CODEC_VIDEO_DECODER_VP8_H_
