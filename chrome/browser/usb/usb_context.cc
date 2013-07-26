// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/usb/usb_context.h"

#include "base/logging.h"
#include "base/threading/platform_thread.h"
#include "third_party/libusb/src/libusb/interrupt.h"
#include "third_party/libusb/src/libusb/libusb.h"

// The UsbEventHandler works around a design flaw in the libusb interface. There
// is currently no way to signal to libusb that any caller into one of the event
// handler calls should return without handling any events.
class UsbEventHandler : public base::PlatformThread::Delegate {
 public:
  explicit UsbEventHandler(libusb_context* context)
      : running_(true),
        context_(context),
        thread_handle_(0) {
    base::PlatformThread::Create(0, this, &thread_handle_);
  }

  virtual ~UsbEventHandler() {}

  virtual void ThreadMain() OVERRIDE {
    base::PlatformThread::SetName("UsbEventHandler");
    VLOG(1) << "UsbEventHandler started.";
    while (running_)
      libusb_handle_events(context_);
    VLOG(1) << "UsbEventHandler shutting down.";
  }

  void Stop() {
    running_ = false;
    base::subtle::MemoryBarrier();
    libusb_interrupt_handle_event(context_);
    base::PlatformThread::Join(thread_handle_);
  }

 private:
  volatile bool running_;
  libusb_context* context_;
  base::PlatformThreadHandle thread_handle_;
  DISALLOW_COPY_AND_ASSIGN(UsbEventHandler);
};


UsbContext::UsbContext() : context_(NULL) {
  CHECK_EQ(0, libusb_init(&context_)) << "Cannot initialize libusb";
  event_handler_.reset(new UsbEventHandler(context_));
}

UsbContext::~UsbContext() {
  event_handler_->Stop();
  event_handler_.reset(NULL);
  libusb_exit(context_);
}
