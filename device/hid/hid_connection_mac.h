// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_HID_HID_CONNECTION_MAC_H_
#define DEVICE_HID_HID_CONNECTION_MAC_H_

#include "device/hid/hid_connection.h"

#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/hid/IOHIDManager.h>

namespace net {
class IOBuffer;
}

namespace device {

typedef base::Callback<
    void(bool success, scoped_refptr<net::IOBuffer> buffer, size_t size)>
    HidReadCallback;
typedef base::Callback<void(bool success)> HidWriteCallback;

class HidConnectionMac : public HidConnection {
 public:
  virtual void Read(const HidReadCallback& callback) OVERRIDE;
  virtual void Write(uint8 report_id,
                     scoped_refptr<net::IOBuffer> buffer,
                     size_t size,
                     const HidWriteCallback& callback) OVERRIDE;
  virtual void GetFeatureReport(const HidReadCallback& callback) OVERRIDE;
  virtual void SendFeatureReport(uint8 report_id,
                                 scoped_refptr<net::IOBuffer> buffer,
                                 size_t size,
                                 const HidWriteCallback& callback) OVERRIDE;

 private:
  friend class HidService;

  HidConnectionMac(IOHIDDeviceRef device);
  virtual ~HidConnectionMac();

  base::ScopedCFTypeRef<IOHIDDeviceRef> device_;

  DISALLOW_COPY_AND_ASSIGN(HidConnectionMac);
};

}  // namespace device

#endif  // DEVICE_HID_HID_CONNECTION_H_
