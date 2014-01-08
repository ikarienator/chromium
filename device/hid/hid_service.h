// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_HID_HID_SERVICE_H_
#define DEVICE_HID_HID_SERVICE_H_

#include <map>
#include <string>
#include <vector>

#include "build/build_config.h"
#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/memory/singleton.h"
#include "base/strings/string16.h"
#include "base/threading/thread_checker.h"

template <typename T> struct DefaultSingletonTraits;

namespace device {

class HidConnection;
class HidDevice;

class HidService {
 public:
  // Must be called on an IO-Allowed thread.
  static HidService* GetInstance();

  void UpdateDevices();
  void GetDevices(std::vector<scoped_refptr<HidDevice> >* devices) const;

  // Returns empty pointer if not found.
  scoped_refptr<HidDevice> GetDevice(int device_id) const;

 private:
  friend struct DefaultSingletonTraits<HidService>;

  HidService();
  ~HidService();

  int next_unique_id_;

  typedef std::map<int, scoped_refptr<HidDevice> > DeviceMap;
  DeviceMap devices_;

  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(HidService);
};

}  // namespace device

#endif  // DEVICE_HID_HID_SERVICE_H_
