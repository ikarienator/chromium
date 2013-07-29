// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_USB_USB_CONTEXT_H_
#define CHROME_BROWSER_USB_USB_CONTEXT_H_

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/synchronization/waitable_event.h"
#include "content/public/browser/browser_thread.h"

class UsbEventHandler;
struct libusb_context;

typedef libusb_context* PlatformUsbContext;

// Ref-counted wrapper for libusb_context*.
// It also manages the life-cycle of UsbEventHandler.
// It is a blocking operation to delete UsbContext.
// Destructor must be called on FILE thread.
class UsbContext
    : public base::RefCountedThreadSafe<
          UsbContext, content::BrowserThread::DeleteOnFileThread> {
 public:
  PlatformUsbContext context() const { return context_; }

 protected:
  friend class UsbService;
  friend struct content::BrowserThread::DeleteOnThread<
      content::BrowserThread::FILE>;
  friend class base::DeleteHelper<UsbContext>;

  // Set wait_for_polling_starts to true if the constructor should wait until
  // the polling starts. It will block the current thread. Only use it in tests.
  explicit UsbContext(bool wait_for_polling_starts);
  virtual ~UsbContext();

 private:
  PlatformUsbContext context_;
  scoped_ptr<UsbEventHandler> event_handler_;

  DISALLOW_COPY_AND_ASSIGN(UsbContext);
};

#endif  // CHROME_BROWSER_USB_USB_CONTEXT_H_
