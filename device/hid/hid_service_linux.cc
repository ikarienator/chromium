// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/hid/hid_service.h"

#include <map>
#include <set>
#include <string>
#include <vector>
#include <libudev.h>
#include <linux/hidraw.h>
#include <linux/version.h>
#include <linux/input.h>
#include <sys/stat.h>

#include "base/logging.h"
#include "base/memory/scoped_vector.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "content/browser/udev_linux.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace device {

namespace {

const char* kUdevActionAdd = "add";
const char* kUdevActionRemove = "remove";
const char* kHIDSubSystem = "hid";

const char* kHIDID = "HID_ID";
const char* kHIDName = "HID_NAME";
const char* kHIDUnique = "HID_UNIQ";

template<typename T, void func(T*)>
struct GeneralDeleter {
  void operator()(T* enumerate) const {
    if (enumerate != NULL)
      func(enumerate);
  }
};

typedef GeneralDeleter<udev_enumerate, udev_enumerate_unref> UdevEnumerateDeleter;
typedef GeneralDeleter<udev_device, udev_device_unref> UdevDeviceDeleter;

}  // namespace

class HidDeviceWatcherLinux : public HidDeviceWatcher {
 public:
  HidDeviceWatcherLinux(HidService* service);
  void OnDeviceChange(udev_device* raw_dev) const;

  void Enumerate() const;

 private:
  virtual ~HidDeviceWatcherLinux();

  void DeviceAdd(udev_device* device) const;
  void DeviceRemove(udev_device* raw_dev) const;

  scoped_ptr<content::UdevLinux> udev_;

  DISALLOW_COPY_AND_ASSIGN(HidDeviceWatcherLinux);
};

HidDeviceWatcherLinux::HidDeviceWatcherLinux(HidService* service)
    : HidDeviceWatcher(service) {
  std::vector<content::UdevLinux::UdevMonitorFilter> filters;
  filters.push_back(content::UdevLinux::UdevMonitorFilter(kHIDSubSystem, NULL));
  udev_.reset(new content::UdevLinux(
      filters, base::Bind(&HidDeviceWatcherLinux::OnDeviceChange, this)));
  Enumerate();
}

HidDeviceWatcherLinux::~HidDeviceWatcherLinux() {}


void HidDeviceWatcherLinux::Enumerate() const {
  udev* udev = udev_->udev_handle();
  scoped_ptr<udev_enumerate, UdevEnumerateDeleter> enumerate(
      udev_enumerate_new(udev));

  udev_enumerate_scan_devices(enumerate.get());
  udev_list_entry* devices = udev_enumerate_get_list_entry(enumerate.get());

  for (udev_list_entry* i = devices; i != NULL;
       i = udev_list_entry_get_next(i)) {
    scoped_ptr<udev_device, UdevDeviceDeleter> hid_dev(
        udev_device_new_from_syspath(udev, udev_list_entry_get_name(i)));
    if (hid_dev) {
      DeviceAdd(hid_dev.get());
    }
  }
}

void HidDeviceWatcherLinux::OnDeviceChange(udev_device* raw_dev) const {
  DCHECK(raw_dev);

  std::string action(udev_device_get_action(raw_dev));
  // std::string subsystem(udev_device_get_subsystem(device));

  if (action == kUdevActionAdd) {
    DeviceAdd(raw_dev);
  } else if (action == kUdevActionRemove) {
    DeviceRemove(raw_dev);
  } else {
    LOG(ERROR) << action;
  }
}

void HidDeviceWatcherLinux::DeviceAdd(udev_device* raw_device) const {
  udev_device* device =
      udev_device_get_parent_with_subsystem_devtype(raw_device, "hid", NULL);

  if (!device)
    return;

  HidDeviceInfo device_info;
  uint32 int_property = 0;
  const char* str_property = NULL;

  str_property = udev_device_get_syspath(device);
  if (str_property == NULL)
    return;
  std::string device_id = str_property;

  std::string hid_id = udev_device_get_property_value(device, kHIDID);
  std::vector<std::string> parts;
  base::SplitString(hid_id, ':', &parts);
  if (parts.size() != 3) {
    return;
  }

  if (HexStringToUInt(base::StringPiece(parts[1]), &int_property)){
    device_info.vendor_id = int_property;
  }

  if (HexStringToUInt(base::StringPiece(parts[2]), &int_property)){
    device_info.product_id = int_property;
  }

  str_property = udev_device_get_property_value(device, kHIDUnique);
  if (str_property != NULL)
    device_info.serial_number = str_property;

  str_property = udev_device_get_property_value(device, kHIDName);
  if (str_property != NULL)
    device_info.product_name = str_property;

  LOG(ERROR) << device_id;
  HidDeviceWatcher::DeviceAdd(device_info);
}

void HidDeviceWatcherLinux::DeviceRemove(udev_device* raw_dev) const {
  udev_device* hid_dev =
      udev_device_get_parent_with_subsystem_devtype(raw_dev, "hid", NULL);

  if (!hid_dev)
    return;

  const char* device_id = NULL;
  device_id = udev_device_get_syspath(hid_dev);
  if (device_id == NULL)
    return;

  LOG(ERROR) << device_id;
  HidDeviceWatcher::DeviceRemove(device_id);
}

void HidService::InitializeDeviceWatcher() {
  watcher_ = new HidDeviceWatcherLinux(this);
}
}  // namespace dev
