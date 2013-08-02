// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/usb/usb_service.h"

#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "chrome/browser/usb/usb_context.h"
#include "chrome/browser/usb/usb_device_handle.h"
#include "third_party/libusb/src/libusb/libusb.h"

#if defined(OS_CHROMEOS)
#include "base/chromeos/chromeos_version.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/permission_broker_client.h"
#endif  // defined(OS_CHROMEOS)

using std::vector;

UsbService::UsbService() : context_(new UsbContext()) {}

UsbService::~UsbService() {}

UsbService* UsbService::GetInstance() {
  return Singleton<UsbService>::get();
}

void UsbService::FindDevices(const uint16 vendor_id,
                             const uint16 product_id,
                             int interface_id,
                             vector<scoped_refptr<UsbDeviceHandle> >* devices,
                             const base::Callback<void()>& callback) {
#if defined(OS_CHROMEOS)
  // ChromeOS builds on non-ChromeOS machines (dev) should not attempt to
  // use permission broker.
  if (base::chromeos::IsRunningOnChromeOS()) {
    chromeos::PermissionBrokerClient* client =
        chromeos::DBusThreadManager::Get()->GetPermissionBrokerClient();
    DCHECK(client) << "Could not get permission broker client.";
    if (!client) {
      callback.Run();
      return;
    }

    client->RequestUsbAccess(vendor_id,
                             product_id,
                             interface_id,
                             base::Bind(&UsbService::FindDevicesImpl,
                                        base::Unretained(this),
                                        vendor_id,
                                        product_id,
                                        devices,
                                        callback));
  } else {
    FindDevicesImpl(vendor_id, product_id, devices, callback, true);
  }
#else
  FindDevicesImpl(vendor_id, product_id, devices, callback, true);
#endif  // defined(OS_CHROMEOS)
}

void UsbService::EnumerateDevices(
    std::vector<scoped_refptr<UsbDeviceHandle> >* devices) {
  devices->clear();

  DeviceVector enumerated_devices;
  EnumerateDevicesImpl(&enumerated_devices);

  for (DeviceVector::iterator it = enumerated_devices.begin();
       it != enumerated_devices.end(); ++it) {
    PlatformUsbDevice device = it->device();
    UsbDeviceHandle* const wrapper = LookupOrCreateDevice(device);
    if (wrapper)
      devices->push_back(wrapper);
  }
}

void UsbService::FindDevicesImpl(
    const uint16 vendor_id,
    const uint16 product_id,
    vector<scoped_refptr<UsbDeviceHandle> >* devices,
    const base::Callback<void()>& callback,
    bool success) {
  base::ScopedClosureRunner run_callback(callback);

  devices->clear();

  // If the permission broker was unable to obtain permission for the specified
  // devices then there is no point in attempting to enumerate the devices. On
  // platforms without a permission broker, we assume permission is granted.
  if (!success)
    return;

  DeviceVector enumerated_devices;
  EnumerateDevicesImpl(&enumerated_devices);

  for (DeviceVector::iterator it = enumerated_devices.begin();
       it != enumerated_devices.end(); ++it) {
    PlatformUsbDevice device = it->device();
    if (DeviceMatches(device, vendor_id, product_id)) {
      UsbDeviceHandle* const wrapper = LookupOrCreateDevice(device);
      if (wrapper)
        devices->push_back(wrapper);
    }
  }
}

void UsbService::CloseDevice(scoped_refptr<UsbDeviceHandle> device) {
  PlatformUsbDevice platform_device = libusb_get_device(device->handle());
  if (!ContainsKey(devices_, platform_device)) {
    LOG(WARNING) << "CloseDevice called for device we're not tracking!";
    return;
  }

  devices_.erase(platform_device);
  libusb_close(device->handle());
}

UsbService::RefCountedPlatformUsbDevice::RefCountedPlatformUsbDevice(
    PlatformUsbDevice device) : device_(device) {
  libusb_ref_device(device_);
}

UsbService::RefCountedPlatformUsbDevice::RefCountedPlatformUsbDevice(
    const RefCountedPlatformUsbDevice& other) : device_(other.device_) {
  libusb_ref_device(device_);
}

UsbService::RefCountedPlatformUsbDevice::~RefCountedPlatformUsbDevice() {
  libusb_unref_device(device_);
}

PlatformUsbDevice UsbService::RefCountedPlatformUsbDevice::device() {
  return device_;
}

void UsbService::EnumerateDevicesImpl(DeviceVector* output) {
  STLClearObject(output);

  libusb_device** devices = NULL;
  const ssize_t device_count = libusb_get_device_list(
      context_->context(),
      &devices);
  if (device_count < 0)
    return;

  for (int i = 0; i < device_count; ++i) {
    libusb_device* device = devices[i];
    libusb_ref_device(device);
    output->push_back(RefCountedPlatformUsbDevice(device));
  }

  libusb_free_device_list(devices, true);
}

bool UsbService::DeviceMatches(PlatformUsbDevice device,
                               const uint16 vendor_id,
                               const uint16 product_id) {
  libusb_device_descriptor descriptor;
  if (libusb_get_device_descriptor(device, &descriptor))
    return false;
  return descriptor.idVendor == vendor_id && descriptor.idProduct == product_id;
}

UsbDeviceHandle* UsbService::LookupOrCreateDevice(PlatformUsbDevice device) {
  if (!ContainsKey(devices_, device)) {
    libusb_device_handle* handle = NULL;
    if (libusb_open(device, &handle)) {
      LOG(WARNING) << "Could not open device.";
      return NULL;
    }

    UsbDeviceHandle* wrapper = new UsbDeviceHandle(this, handle);
    devices_[device] = wrapper;
  }
  return devices_[device].get();
}
