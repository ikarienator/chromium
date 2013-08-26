// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_HID_HID_DEVICE_H_
#define CHROME_BROWSER_HID_HID_DEVICE_H_

#include "base/memory/ref_counted.h"
#include "base/callback.h"

namespace net {

class IOBuffer;

} // namespace net

typedef base::Callback<void(bool success,
                            scoped_refptr<net::IOBuffer> buffer,
                            size_t size)> HidReadCallback;

typedef base::Callback<void(bool success)> HidWriteCallback;


class HidDevice : public base::RefCounted<HidDevice> {
 public:
  HidDevice();

  virtual void Read(const HidReadCallback& callback);

  virtual void Write(uint8 report_id,
                     scoped_refptr<net::IOBuffer> buffer,
                     size_t size,
                     const HidWriteCallback& callback);

  virtual void GetFeatureReport(const HidReadCallback& callback);

  virtual void SendFeatureReport(uint8 report_id,
                                 scoped_refptr<net::IOBuffer> buffer,
                                 size_t size,
                                 const HidWriteCallback& callback);

 private:
  friend class base::RefCounted<HidDevice>;

  virtual ~HidDevice();

  DISALLOW_COPY_AND_ASSIGN(HidDevice);
};

#endif  // CHROME_BROWSER_HID_HID_DEVICE_H_
