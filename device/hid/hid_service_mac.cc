// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/hid/hid_service.h"

#include <map>
#include <set>
#include <string>

#include "base/logging.h"
#include "base/mac/foundation_util.h"
#include "base/memory/scoped_vector.h"
#include "base/threading/thread_restrictions.h"
#include "base/strings/string_number_conversions.h"
#include "content/public/browser/browser_thread.h"
#include "device/hid/hid_connection.h"
#include "device/hid/hid_utils_mac.h"

#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/hid/IOHIDManager.h>

using content::BrowserThread;

namespace device {

class HidDeviceWatcherMac : public HidDeviceWatcher {
 public:
  HidDeviceWatcherMac(HidService* service);

  static void DeviceAddCallback(void* context,
                                IOReturn result,
                                void* sender,
                                IOHIDDeviceRef ref);
  static void DeviceRemoveCallback(void* context,
                                   IOReturn result,
                                   void* sender,
                                   IOHIDDeviceRef ref);
  static HidDeviceWatcherMac* InstanceFromContext(void* context);

  IOHIDDeviceRef FindDevice(std::string id);

 private:
  virtual ~HidDeviceWatcherMac();

  void Enumerate() const;

  void DeviceAdd(IOHIDDeviceRef ref) const;
  void DeviceRemove(IOHIDDeviceRef ref) const;


  HidService* service_;
  base::ScopedCFTypeRef<IOHIDManagerRef> hid_manager_ref_;
  bool enabled_;

  DISALLOW_COPY_AND_ASSIGN(HidDeviceWatcherMac);
};

HidDeviceWatcherMac::HidDeviceWatcherMac(HidService* service)
    : HidDeviceWatcher(service),
      enabled_(false) {
  hid_manager_ref_.reset(IOHIDManagerCreate(kCFAllocatorDefault,
                                            kIOHIDOptionsTypeNone));
  if (CFGetTypeID(hid_manager_ref_) != IOHIDManagerGetTypeID()) {
    return;
  }

  // Register for plug/unplug notifications.
  IOHIDManagerSetDeviceMatching(hid_manager_ref_.get(), NULL);
  IOHIDManagerRegisterDeviceMatchingCallback(
      hid_manager_ref_.get(),
      &HidDeviceWatcherMac::DeviceAddCallback,
      this);
  IOHIDManagerRegisterDeviceRemovalCallback(
      hid_manager_ref_.get(),
      &HidDeviceWatcherMac::DeviceRemoveCallback,
      this);

  CFRunLoopRef runloop = CFRunLoopGetCurrent();

  IOHIDManagerScheduleWithRunLoop(
      hid_manager_ref_,
      runloop,
      kCFRunLoopDefaultMode);

  enabled_ = IOHIDManagerOpen(hid_manager_ref_,
                              kIOHIDOptionsTypeNone) == kIOReturnSuccess;

  Enumerate();
}

HidDeviceWatcherMac::~HidDeviceWatcherMac() {
  if (enabled_)
    IOHIDManagerClose(hid_manager_ref_, kIOHIDOptionsTypeNone);
}

HidDeviceWatcherMac* HidDeviceWatcherMac::InstanceFromContext(void* context) {
  return reinterpret_cast<HidDeviceWatcherMac*>(context);
}

void HidDeviceWatcherMac::DeviceAddCallback(void* context,
                                            IOReturn result,
                                            void* sender,
                                            IOHIDDeviceRef ref) {
  InstanceFromContext(context)->DeviceAdd(ref);
}
void HidDeviceWatcherMac::DeviceRemoveCallback(void* context,
                                               IOReturn result,
                                               void* sender,
                                               IOHIDDeviceRef ref) {
  InstanceFromContext(context)->DeviceRemove(ref);
}

IOHIDDeviceRef HidDeviceWatcherMac::FindDevice(std::string id) {
  base::ScopedCFTypeRef<CFSetRef> devices(
      IOHIDManagerCopyDevices(hid_manager_ref_));
  CFIndex count = CFSetGetCount(devices);
  scoped_ptr<IOHIDDeviceRef[]> device_refs(new IOHIDDeviceRef[count]);
  CFSetGetValues(devices, (const void **)(device_refs.get()));

  for (CFIndex i = 0; i < count; i++) {
    int32_t int_property = 0;
    if (GetIntProperty(device_refs[i], CFSTR(kIOHIDLocationIDKey),
                       &int_property)) {
      if (id == base::HexEncode(&int_property, sizeof(int_property))) {
        return device_refs[i];
      }
    }
  }

  return NULL;
}

void HidDeviceWatcherMac::Enumerate() const {
  return;
  base::ScopedCFTypeRef<CFSetRef> devices(
      IOHIDManagerCopyDevices(hid_manager_ref_));
  CFIndex count = CFSetGetCount(devices);
  scoped_ptr<IOHIDDeviceRef[]> device_refs(new IOHIDDeviceRef[count]);
  CFSetGetValues(devices, (const void **)(device_refs.get()));

  for (CFIndex i = 0; i < count; i++) {
    DeviceAdd(device_refs[i]);
    CFRelease(device_refs[i]);
  }
}

void HidDeviceWatcherMac::DeviceAdd(IOHIDDeviceRef ref) const {
  HidDeviceInfo device;
  int32_t int_property = 0;
  std::string str_property;
  std::string device_id;

  // Unique identifier for HID device.
  if (GetIntProperty(ref, CFSTR(kIOHIDLocationIDKey), &int_property)) {
    device_id = base::HexEncode(&int_property, sizeof(int_property));
  } else {
    // Not an available device.
    return;
  }

  if (GetIntProperty(ref, CFSTR(kIOHIDVendorIDKey), &int_property)) {
    device.vendor_id = int_property;
  }
  if (GetIntProperty(ref, CFSTR(kIOHIDProductIDKey), &int_property)) {
    device.product_id = int_property;
  }
  if (GetIntProperty(ref, CFSTR(kIOHIDPrimaryUsageKey), &int_property)) {
    device.usage = int_property;
  }
  if (GetIntProperty(ref, CFSTR(kIOHIDPrimaryUsagePageKey), &int_property)) {
    device.usage_page = int_property;
  }
  if (GetStringProperty(ref, CFSTR(kIOHIDProductKey), &str_property)) {
    device.product_name = str_property;
  }
  if (GetStringProperty(ref, CFSTR(kIOHIDSerialNumberKey), &str_property)) {
    device.serial_number = str_property;
  }

  HidDeviceWatcher::DeviceAdd(device);
}

void HidDeviceWatcherMac::DeviceRemove(IOHIDDeviceRef ref) const {
  std::string device_id;
  if (!GetStringProperty(ref, CFSTR(kIOHIDTransportKey), &device_id)) {
    return;
  }

  HidDeviceWatcher::DeviceRemove(device_id);
}

void HidService::InitializeDeviceWatcher() {
  watcher_ = new HidDeviceWatcherMac(this);
}

scoped_refptr<HidConnection> HidService::Connect(std::string device_id) const {
//  IOHIDDeviceRef ref =
//      static_cast<HidDeviceWatcherMac*>(watcher_.get())->FindDevice(device_id);
 //  if (ref == NULL)
    return scoped_refptr<HidConnection>();

  // return make_scoped_refptr(new HidConnection());
}

}  // namespace device
