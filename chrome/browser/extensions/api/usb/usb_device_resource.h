// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_USB_USB_DEVICE_RESOURCE_H_
#define CHROME_BROWSER_EXTENSIONS_API_USB_USB_DEVICE_RESOURCE_H_

#include <set>

#include "base/basictypes.h"
#include "base/memory/linked_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/synchronization/lock.h"
#include "chrome/browser/extensions/api/api_resource.h"
#include "chrome/browser/extensions/api/api_resource_manager.h"
#include "chrome/browser/usb/usb_device_handle.h"
#include "chrome/common/extensions/api/usb.h"

class UsbDeviceHandle;

namespace net {
class IOBuffer;
}  // namespace net

namespace extensions {

// A UsbDeviceResource is an ApiResource wrapper for a UsbDevice.
// It will redirect all the requests on the FILE thread.
class UsbDeviceResource : public ApiResource {
 public:
  UsbDeviceResource(const std::string& owner_extension_id,
                    scoped_refptr<UsbDeviceHandle> device);
  virtual ~UsbDeviceResource();

  virtual void Close(const base::Callback<void()>& callback);

  virtual void ListInterfaces(UsbConfigDescriptor* config,
                              const UsbInterfaceCallback& callback);

  virtual void ClaimInterface(const int interface_number,
                              const UsbInterfaceCallback& callback);

  virtual void ReleaseInterface(const int interface_number,
                                const UsbInterfaceCallback& callback);

  virtual void SetInterfaceAlternateSetting(
      const int interface_number, const int alternate_setting,
      const UsbInterfaceCallback& callback);

  virtual void ControlTransfer(
      const UsbEndpointDirection direction,
      const UsbDeviceHandle::TransferRequestType request_type,
      const UsbDeviceHandle::TransferRecipient recipient, const uint8 request,
      const uint16 value, const uint16 index, net::IOBuffer* buffer,
      const size_t length, const unsigned int timeout,
      const UsbTransferCallback& callback);

  virtual void BulkTransfer(const UsbEndpointDirection direction,
                            const uint8 endpoint, net::IOBuffer* buffer,
                            const size_t length, const unsigned int timeout,
                            const UsbTransferCallback& callback);

  virtual void InterruptTransfer(const UsbEndpointDirection direction,
                                 const uint8 endpoint, net::IOBuffer* buffer,
                                 const size_t length,
                                 const unsigned int timeout,
                                 const UsbTransferCallback& callback);

  virtual void IsochronousTransfer(const UsbEndpointDirection direction,
                                   const uint8 endpoint, net::IOBuffer* buffer,
                                   const size_t length,
                                   const unsigned int packets,
                                   const unsigned int packet_length,
                                   const unsigned int timeout,
                                   const UsbTransferCallback& callback);

  virtual void ResetDevice(const base::Callback<void(bool)>& callback);

 private:
  friend class ApiResourceManager<UsbDeviceResource>;
  static const char* service_name() {
    return "UsbDeviceResourceManager";
  }

  scoped_refptr<UsbDeviceHandle> device_;

  DISALLOW_COPY_AND_ASSIGN(UsbDeviceResource);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_USB_USB_DEVICE_RESOURCE_H_
