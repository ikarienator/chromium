// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_HID_HID_DEVICE_H_
#define DEVICE_HID_HID_DEVICE_H_

#include <string>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/threading/thread_restrictions.h"

struct hid_device_info;

namespace device {

typedef struct hid_device_info* PlatformHidDeviceInfo;

enum HidBusType {
  kHIDBusTypeUSB = 0,
  kHIDBusTypeBluetooth = 1,
};

class HidConnection;
class HidService;

class HidDevice : public base::RefCountedThreadSafe<HidDevice> {
 public:
  uint32 device_id() const { return device_id_; }
  HidBusType bus_type() const { return bus_type_; }
  std::string path() const { return path_; }
  uint16 vendor_id() const { return vendor_id_; }
  uint16 product_id() const { return product_id_; }
  uint16 release_number() const { return release_number_; }
  uint16 usage_page() const { return usage_page_; }
  uint16 usage() const { return usage_; }
  uint16 interface_number() const { return interface_number_; }

  scoped_refptr<HidConnection> Connect();

 private:
  friend class base::RefCountedThreadSafe<HidDevice>;
  friend class HidService;

  HidDevice(uint32 device_id, PlatformHidDeviceInfo device_info);
  ~HidDevice();

  uint32 device_id_;
  HidBusType bus_type_;
  std::string path_;
  uint16 vendor_id_;
  uint16 product_id_;
  uint16 release_number_;
  uint16 usage_page_;
  uint16 usage_;
  uint16 interface_number_;

  DISALLOW_COPY_AND_ASSIGN(HidDevice);
};

}  // namespace device

#endif  // DEVICE_HID_HID_DEVICE_H_
