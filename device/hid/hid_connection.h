// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_HID_HID_CONNECTION_H_
#define DEVICE_HID_HID_CONNECTION_H_

#include "base/memory/ref_counted.h"
#include "base/callback.h"

namespace net {
class IOBuffer;
}

namespace device {

typedef base::Callback<
    void(bool success, scoped_refptr<net::IOBuffer> buffer, size_t size)>
    HidReadCallback;
typedef base::Callback<void(bool success)> HidWriteCallback;

class HidConnection : public base::RefCounted<HidConnection> {
 public:
  virtual void Read(const HidReadCallback& callback) = 0;
  virtual void Write(uint8 report_id,
                     scoped_refptr<net::IOBuffer> buffer,
                     size_t size,
                     const HidWriteCallback& callback) = 0;
  virtual void GetFeatureReport(const HidReadCallback& callback) = 0;
  virtual void SendFeatureReport(uint8 report_id,
                                 scoped_refptr<net::IOBuffer> buffer,
                                 size_t size,
                                 const HidWriteCallback& callback) = 0;

 protected:
  friend class base::RefCounted<HidConnection>;
  friend class HidDevice;

  HidConnection();
  virtual ~HidConnection();

  DISALLOW_COPY_AND_ASSIGN(HidConnection);
};

}  // namespace device

#endif  // DEVICE_HID_HID_CONNECTION_H_
