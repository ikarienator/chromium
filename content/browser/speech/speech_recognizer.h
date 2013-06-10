// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SPEECH_SPEECH_RECOGNIZER_H_
#define CONTENT_BROWSER_SPEECH_SPEECH_RECOGNIZER_H_

#include "base/memory/ref_counted.h"

namespace content {

class SpeechRecognitionEventListener;

// Handles speech recognition for a session (identified by |session_id|).
class CONTENT_EXPORT SpeechRecognizer
    : public base::RefCountedThreadSafe<SpeechRecognizer> {
 public:

  SpeechRecognizer(SpeechRecognitionEventListener* listener, int session_id)
      : listener_(listener), session_id_(session_id) {}

  virtual void StartRecognition() = 0;
  virtual void AbortRecognition() = 0;
  virtual void StopAudioCapture() = 0;
  virtual bool IsActive() const = 0;
  virtual bool IsCapturingAudio() const = 0;

 protected:
  virtual ~SpeechRecognizer() {}
  SpeechRecognitionEventListener* listener() const { return listener_; }
  int session_id() const { return session_id_; }

 private:
  friend class base::RefCountedThreadSafe<SpeechRecognizer>;

  SpeechRecognitionEventListener* listener_;
  int session_id_;

  DISALLOW_COPY_AND_ASSIGN(SpeechRecognizer);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SPEECH_SPEECH_RECOGNIZER_H_
