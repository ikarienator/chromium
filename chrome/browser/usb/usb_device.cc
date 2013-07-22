// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/usb/usb_device.h"

#include <vector>

#include "base/stl_util.h"
#include "base/synchronization/lock.h"
#include "chrome/browser/usb/usb_interface.h"
#include "chrome/browser/usb/usb_service.h"
#include "chrome/browser/usb/usb_transfer.h"
#include "third_party/libusb/src/libusb/libusb.h"

UsbDevice::UsbDevice(UsbService* service, PlatformUsbDeviceHandle handle)
    : service_(service), handle_(handle) {
  DCHECK(handle) << "Cannot create device with NULL handle.";
}

UsbDevice::UsbDevice() : service_(NULL), handle_(NULL) {}

UsbDevice::~UsbDevice() {}

void UsbDevice::Close(const base::Callback<void()>& callback) {
  CheckDevice();
  for (TransferSet::iterator it = transfers_.begin();
      it != transfers_.end();
      it++) {
    (*it)->Abort();
  }
  service_->CloseDevice(this);
  handle_ = NULL;
  callback.Run();
}

void UsbDevice::ListInterfaces(UsbConfigDescriptor* config,
                               const UsbInterfaceCallback& callback) {
  CheckDevice();

  PlatformUsbDevice device = libusb_get_device(handle_);

  PlatformUsbConfigDescriptor platform_config;
  const int list_result = libusb_get_active_config_descriptor(device,
      &platform_config);
  if (list_result == 0) {
    config->Reset(platform_config);
  }
  callback.Run(list_result == 0);
}

void UsbDevice::ClaimInterface(const int interface_number,
                               const UsbInterfaceCallback& callback) {
  CheckDevice();

  const int claim_result = libusb_claim_interface(handle_, interface_number);
  callback.Run(claim_result == 0);
}

void UsbDevice::ReleaseInterface(const int interface_number,
                                 const UsbInterfaceCallback& callback) {
  CheckDevice();

  const int release_result = libusb_release_interface(handle_,
                                                      interface_number);
  callback.Run(release_result == 0);
}

void UsbDevice::SetInterfaceAlternateSetting(
    const int interface_number,
    const int alternate_setting,
    const UsbInterfaceCallback& callback) {
  CheckDevice();

  const int setting_result = libusb_set_interface_alt_setting(handle_,
      interface_number, alternate_setting);

  callback.Run(setting_result == 0);
}

void UsbDevice::ResetDevice(const base::Callback<void(bool)>& callback) {
  CheckDevice();
  callback.Run(libusb_reset_device(handle_) == 0);
}

void UsbDevice::CheckDevice() {
  DCHECK(handle_) << "Device is already closed.";
}

void UsbDevice::RegisterTransfer(UsbTransfer* transfer) {
  transfers_.insert(transfer);
}
void UsbDevice::UnregisterTransfer(UsbTransfer* transfer) {
  transfers_.erase(transfer);
}
