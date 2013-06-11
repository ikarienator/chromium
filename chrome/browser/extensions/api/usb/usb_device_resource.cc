// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/usb/usb_device_resource.h"

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/synchronization/lock.h"
#include "chrome/browser/extensions/api/api_resource.h"
#include "chrome/browser/usb/usb_device_handle.h"
#include "chrome/common/extensions/api/usb.h"
#include "content/public/browser/browser_thread.h"

namespace extensions {

static base::LazyInstance<ProfileKeyedAPIFactory<
        ApiResourceManager<UsbDeviceResource> > >
            g_factory = LAZY_INSTANCE_INITIALIZER;

// static
template <>
ProfileKeyedAPIFactory<ApiResourceManager<UsbDeviceResource> >*
ApiResourceManager<UsbDeviceResource>::GetFactoryInstance() {
  return &g_factory.Get();
}

namespace {

void EmptyCallback(void) {}

}  // namepsace

UsbDeviceResource::UsbDeviceResource(const std::string& owner_extension_id,
                                     scoped_refptr<UsbDeviceHandle> device)
    : ApiResource(owner_extension_id), device_(device) {}

UsbDeviceResource::~UsbDeviceResource() { Close(base::Bind(EmptyCallback)); }

void UsbDeviceResource::Close(const base::Callback<void()>& callback) {
  scoped_refptr<UsbDeviceHandle> handle;
  handle.swap(device_);
  if (!handle.get()) {
    callback.Run();
    return;
  }
  content::BrowserThread::PostTask(
      content::BrowserThread::FILE, FROM_HERE,
      base::Bind(&UsbDeviceHandle::Close, handle, callback));
}

void UsbDeviceResource::ListInterfaces(UsbConfigDescriptor* config,
                                       const UsbInterfaceCallback& callback) {
  if (!device_.get()) {
    callback.Run(false);
  }
  content::BrowserThread::PostTask(
      content::BrowserThread::FILE, FROM_HERE,
      base::Bind(&UsbDeviceHandle::ListInterfaces, device_,
                 make_scoped_refptr(config), callback));
}

void UsbDeviceResource::ClaimInterface(const int interface_number,
                                       const UsbInterfaceCallback& callback) {
  if (!device_.get()) {
    callback.Run(false);
  }
  content::BrowserThread::PostTask(
      content::BrowserThread::FILE, FROM_HERE,
      base::Bind(&UsbDeviceHandle::ClaimInterface, device_, interface_number,
                 callback));
}

void UsbDeviceResource::ReleaseInterface(const int interface_number,
                                         const UsbInterfaceCallback& callback) {
  if (!device_.get()) {
    callback.Run(false);
  }
  content::BrowserThread::PostTask(
      content::BrowserThread::FILE, FROM_HERE,
      base::Bind(&UsbDeviceHandle::ReleaseInterface, device_, interface_number,
                 callback));
}

void UsbDeviceResource::SetInterfaceAlternateSetting(
    const int interface_number, const int alternate_setting,
    const UsbInterfaceCallback& callback) {
  if (!device_.get()) {
    callback.Run(false);
  }
  content::BrowserThread::PostTask(
      content::BrowserThread::FILE, FROM_HERE,
      base::Bind(&UsbDeviceHandle::SetInterfaceAlternateSetting, device_,
                 interface_number, alternate_setting, callback));
}

void UsbDeviceResource::ControlTransfer(
    const UsbEndpointDirection direction,
    const UsbDeviceHandle::TransferRequestType request_type,
    const UsbDeviceHandle::TransferRecipient recipient, const uint8 request,
    const uint16 value, const uint16 index, net::IOBuffer* buffer,
    const size_t length, const unsigned int timeout,
    const UsbTransferCallback& callback) {
  if (!device_.get()) {
    callback.Run(USB_TRANSFER_DISCONNECT,
                 scoped_refptr<net::IOBuffer>(), 0);
  }
  device_->ControlTransfer(direction, request_type, recipient, request, value,
                           index, buffer, length, timeout, callback);
}

void UsbDeviceResource::BulkTransfer(const UsbEndpointDirection direction,
                                     const uint8 endpoint,
                                     net::IOBuffer* buffer, const size_t length,
                                     const unsigned int timeout,
                                     const UsbTransferCallback& callback) {
  if (!device_.get()) {
    callback.Run(USB_TRANSFER_DISCONNECT,
                 scoped_refptr<net::IOBuffer>(), 0);
  }
  device_->BulkTransfer(direction, endpoint, buffer, length, timeout, callback);
}

void UsbDeviceResource::InterruptTransfer(const UsbEndpointDirection direction,
                                          const uint8 endpoint,
                                          net::IOBuffer* buffer,
                                          const size_t length,
                                          const unsigned int timeout,
                                          const UsbTransferCallback& callback) {
  if (!device_.get()) {
    callback.Run(USB_TRANSFER_DISCONNECT,
                 scoped_refptr<net::IOBuffer>(), 0);
  }
  device_->InterruptTransfer(direction, endpoint, buffer, length, timeout,
                             callback);
}

void UsbDeviceResource::IsochronousTransfer(
    const UsbEndpointDirection direction, const uint8 endpoint,
    net::IOBuffer* buffer, const size_t length, const unsigned int packets,
    const unsigned int packet_length, const unsigned int timeout,
    const UsbTransferCallback& callback) {
  if (!device_.get()) {
    callback.Run(USB_TRANSFER_DISCONNECT,
                 scoped_refptr<net::IOBuffer>(), 0);
  }
  device_->IsochronousTransfer(direction, endpoint, buffer, length, packets,
                               packet_length, timeout, callback);
}

void UsbDeviceResource::ResetDevice(
    const base::Callback<void(bool)>& callback) {
  if (!device_.get()) {
    callback.Run(false);
  }
  content::BrowserThread::PostTask(
      content::BrowserThread::FILE, FROM_HERE,
      base::Bind(&UsbDeviceHandle::ResetDevice, device_, callback));
}

}  // namespace extensions
