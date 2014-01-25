// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "device/hid/hid_connection.h"
#include "device/hid/hid_service.h"
#include "net/base/io_buffer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device {

namespace {

const int kUSBLUFADemoVID = 0x03eb;
const int kUSBLUFADemoPID = 0x204f;

}  // namespace

TEST(HidServiceTest, Create) {
  base::MessageLoopForIO message_loop;
  HidService* service = HidService::GetInstance();
  ASSERT_TRUE(service);

  std::vector<HidDeviceInfo> devices;
  service->GetDevices(&devices);
  std::string target_device;
  ASSERT_GT(devices.size(), 0U) << "No device found";

  for (std::vector<HidDeviceInfo>::iterator it = devices.begin();
      it != devices.end();
      ++it) {
    if (it->vendor_id == kUSBLUFADemoVID && it->product_id == kUSBLUFADemoPID) {
      target_device = it->device_id;
      break;
    }
  }

  EXPECT_NE(std::string(""), target_device);
}

}  // namespace device
