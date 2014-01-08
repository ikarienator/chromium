// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/hid/hid_device.h"

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "device/hid/hid_connection.h"
#include "third_party/hidapi/hidapi.h"

namespace device {

HidDevice::HidDevice(uint32 device_id, PlatformHidDeviceInfo device_info)
    : device_id_(device_id),
      bus_type_(kHIDBusTypeUSB),
      path_(device_info->path),
      vendor_id_(device_info->vendor_id),
      product_id_(device_info->product_id),
      release_number_(device_info->release_number),
      usage_page_(device_info->usage_page),
      usage_(device_info->usage),
      interface_number_(device_info->interface_number) {}
HidDevice::~HidDevice() {}

scoped_refptr<HidConnection> HidDevice::Connect() {
  base::ThreadRestrictions::AssertIOAllowed();
  PlatformHidDevice device = hid_open_path(path_.c_str());
  if (!device) return scoped_refptr<HidConnection>();
  return new HidConnection(device);
}

}  // namespace device
