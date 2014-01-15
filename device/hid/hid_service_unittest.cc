// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "device/hid/hid_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device {

class MockHidService : public HidService {
 public:
  MockHidService() : inside_loop_(false), done_(false) {}
  virtual ~MockHidService() {}

  virtual void DeviceAdd(HidDeviceInfo info) OVERRIDE;
  virtual void DeviceRemove(std::string device_id) OVERRIDE;

  void WaitForChange() {
    done_ = false;
    while (!done_) {
      inside_loop_ = true;
      base::MessageLoop::current()->Run();
      inside_loop_ = false;
    }
  }

 private:
  bool inside_loop_;
  bool done_;
};

void MockHidService::DeviceAdd(HidDeviceInfo info) {
  HidService::DeviceAdd(info);
  if (inside_loop_)
    base::MessageLoop::current()->Quit();
  done_ = true;
}

void MockHidService::DeviceRemove(std::string device_id) {
  HidService::DeviceRemove(device_id);
  if (inside_loop_)
    base::MessageLoop::current()->Quit();
  done_ = true;
}

TEST(HidServiceTest, Create) {
#if defined(OS_MACOSX)
  base::MessageLoopForUI message_loop;
#else
  base::MessageLoopForIO message_loop;
#endif  // defined(OS_MACOSX_
  scoped_ptr<MockHidService> service(new MockHidService());

  EXPECT_TRUE(service);

  service->WaitForChange();

  std::vector<HidDeviceInfo> devices;
  service->GetDevices(&devices);
  EXPECT_NE(0U, devices.size());
}

}  // namespace device
