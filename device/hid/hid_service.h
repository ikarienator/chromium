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
#include "base/message_loop/message_loop.h"
#include "base/strings/string16.h"
#include "base/threading/thread_checker.h"

template <typename T> struct DefaultSingletonTraits;

namespace device {

class HidConnection;
class HidService;

enum HidBusType {
  kHIDBusTypeUSB = 0,
  kHIDBusTypeBluetooth = 1,
};

struct HidDeviceInfo {
  HidDeviceInfo();
  ~HidDeviceInfo();

  uint16 vendor_id;
  uint16 product_id;
  uint16 usage_page;
  uint16 usage;

  std::string device_id;
  std::string product_name;
  std::string serial_number;
};

class HidDeviceWatcher : public base::RefCountedThreadSafe<HidDeviceWatcher> {
 public:
  HidDeviceWatcher(HidService* service);

 protected:
  friend class base::RefCountedThreadSafe<HidDeviceWatcher>;

  virtual ~HidDeviceWatcher();

  void DeviceAdd(HidDeviceInfo device) const;
  void DeviceRemove(std::string device_id) const;

  HidService* service_;
  DISALLOW_COPY_AND_ASSIGN(HidDeviceWatcher);
};

class HidService : public base::MessageLoop::DestructionObserver {
 public:
  // Must be called on FILE thread.
  static HidService* GetInstance();

  // Enumerates and returns a list of device identifiers.
  void GetDevices(std::vector<HidDeviceInfo>* devices);

  virtual scoped_refptr<HidConnection> Connect(std::string device_id) const;

  virtual void WillDestroyCurrentMessageLoop() OVERRIDE;

 protected:
  friend class HidDeviceWatcher;
  friend struct DefaultSingletonTraits<HidService>;

  HidService();
  explicit HidService(scoped_refptr<HidDeviceWatcher> watcher);
  virtual ~HidService();

  virtual void DeviceAdd(HidDeviceInfo info);
  virtual void DeviceRemove(std::string device_id);

  void InitializeDeviceWatcher();

  typedef std::map<std::string, HidDeviceInfo> DeviceMap;
  DeviceMap devices_;

  scoped_refptr<HidDeviceWatcher> watcher_;

  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(HidService);
};

}  // namespace device

#endif  // DEVICE_HID_HID_SERVICE_H_
