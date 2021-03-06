// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "media/cast/audio_sender/audio_encoder.h"
#include "media/cast/cast_config.h"
#include "media/cast/cast_thread.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {
namespace cast {

static const int64 kStartMillisecond = 123456789;

using base::RunLoop;

static void RelaseFrame(const PcmAudioFrame* frame) {
  delete frame;
};

static void FrameEncoded(scoped_ptr<EncodedAudioFrame> encoded_frame,
                         const base::TimeTicks& recorded_time) {
}

class AudioEncoderTest : public ::testing::Test {
 protected:
  AudioEncoderTest() {}

  virtual void SetUp() {
    cast_thread_ = new CastThread(MessageLoopProxy::current(),
                                  MessageLoopProxy::current(),
                                  MessageLoopProxy::current(),
                                  MessageLoopProxy::current(),
                                  MessageLoopProxy::current());
    AudioSenderConfig audio_config;
    audio_config.codec = kOpus;
    audio_config.use_external_encoder = false;
    audio_config.frequency = 48000;
    audio_config.channels = 2;
    audio_config.bitrate = 64000;
    audio_config.rtp_payload_type = 127;

    audio_encoder_ = new AudioEncoder(cast_thread_, audio_config);
  }

  ~AudioEncoderTest() {}

  base::MessageLoop loop_;
  scoped_refptr<AudioEncoder> audio_encoder_;
  scoped_refptr<CastThread> cast_thread_;
};

TEST_F(AudioEncoderTest, Encode20ms) {
  RunLoop run_loop;

  PcmAudioFrame* audio_frame = new PcmAudioFrame();
  audio_frame->channels = 2;
  audio_frame->frequency = 48000;
  audio_frame->samples.insert(audio_frame->samples.begin(), 480 * 2 * 2, 123);

  base::TimeTicks recorded_time;
  audio_encoder_->InsertRawAudioFrame(audio_frame, recorded_time,
      base::Bind(&FrameEncoded),
      base::Bind(&RelaseFrame, audio_frame));
  run_loop.RunUntilIdle();
}

}  // namespace cast
}  // namespace media
