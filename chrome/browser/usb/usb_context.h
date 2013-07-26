// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_USB_USB_CONTEXT_H_
#define CHROME_BROWSER_USB_USB_CONTEXT_H_

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"

class UsbEventHandler;
struct libusb_context;

typedef libusb_context* PlatformUsbContext;

// Ref-counted wrapper for libusb_context*.
// It also manages the life-cycle of UsbEventHandler
class UsbContext : public base::RefCountedThreadSafe<UsbContext> {
 public:
  UsbContext();
  PlatformUsbContext context() const { return context_; }

 private:
  friend class base::RefCountedThreadSafe<UsbContext>;
  virtual ~UsbContext();
  PlatformUsbContext context_;
  scoped_ptr<UsbEventHandler> event_handler_;
};

#endif  // CHROME_BROWSER_USB_USB_CONTEXT_H_
