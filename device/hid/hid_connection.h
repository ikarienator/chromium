// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_HID_HID_CONNECTION_H_
#define DEVICE_HID_HID_CONNECTION_H_

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/threading/thread_checker.h"

namespace net { class IOBuffer; }

struct hid_device_;

namespace device {

typedef struct hid_device_* PlatformHidDevice;

typedef base::Callback<void(bool success, size_t bytes)> HidIOCallback;

class HidConnection : public base::RefCounted<HidConnection> {
 public:
  void Read(scoped_refptr<net::IOBuffer> buffer,
            size_t size,
            const HidIOCallback& callback);
  void Write(scoped_refptr<net::IOBuffer> buffer,
             size_t size,
             const HidIOCallback& callback);
  void GetFeatureReport(scoped_refptr<net::IOBuffer> buffer,
                        size_t size,
                        const HidIOCallback& callback);
  void SendFeatureReport(scoped_refptr<net::IOBuffer> buffer,
                         size_t size,
                         const HidIOCallback& callback);

 private:
  friend class base::RefCounted<HidConnection>;
  friend class HidDevice;

  explicit HidConnection(PlatformHidDevice platform_device);
  ~HidConnection();

  PlatformHidDevice platform_device_;

  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(HidConnection);
};

}  // namespace device

#endif  // DEVICE_HID_HID_CONNECTION_H_
