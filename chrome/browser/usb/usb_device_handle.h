// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_USB_USB_DEVICE_HANDLE_H_
#define CHROME_BROWSER_USB_USB_DEVICE_HANDLE_H_

#include <map>
#include <vector>

#include "base/memory/ref_counted.h"
#include "base/threading/non_thread_safe.h"
#include "chrome/browser/usb/usb_interface.h"
#include "net/base/completion_callback.h"
#include "net/base/io_buffer.h"
#include "third_party/libusb/src/libusb/libusb.h"

typedef struct libusb_device_handle* PlatformUsbDeviceHandle;
typedef struct libusb_iso_packet_descriptor* PlatformUsbIsoPacketDescriptor;
typedef struct libusb_transfer* PlatformUsbTransferHandle;

class UsbService;

namespace net {
class IOBuffer;
}  // namespace net

enum UsbTransferStatus {
  USB_TRANSFER_COMPLETED = 0,
  USB_TRANSFER_ERROR,
  USB_TRANSFER_TIMEOUT,
  USB_TRANSFER_CANCELLED,
  USB_TRANSFER_STALLED,
  USB_TRANSFER_DISCONNECT,
  USB_TRANSFER_OVERFLOW,
  USB_TRANSFER_LENGTH_SHORT,
};

typedef base::Callback<void(UsbTransferStatus, scoped_refptr<net::IOBuffer>,
                            size_t)> UsbTransferCallback;
typedef base::Callback<void(bool)> UsbInterfaceCallback;

// A UsbDevice wraps the platform's underlying representation of what a USB
// device actually is, and provides accessors for performing many of the
// standard USB operations.
//
// This class should be used on FILE thread.
class UsbDeviceHandle : public base::RefCountedThreadSafe<UsbDeviceHandle>,
                        public base::NonThreadSafe {
 public:
  enum TransferRequestType {
    STANDARD,
    CLASS,
    VENDOR,
    RESERVED
  };
  enum TransferRecipient {
    DEVICE,
    INTERFACE,
    ENDPOINT,
    OTHER
  };

  PlatformUsbDeviceHandle handle() { return handle_; }
  int device() const { return device_; }
  uint16 vendor_id() const { return vendor_id_; }
  uint16 product_id() const { return product_id_; }

  // Close the USB device and release the underlying platform device. |callback|
  // is invoked after the device has been closed.
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

  // The following four methods can be called on any thread.
  virtual void ControlTransfer(const UsbEndpointDirection direction,
                               const TransferRequestType request_type,
                               const TransferRecipient recipient,
                               const uint8 request, const uint16 value,
                               const uint16 index, net::IOBuffer* buffer,
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

 protected:
  // This constructor variant is for use in testing only.
  UsbDeviceHandle();

  friend class base::RefCountedThreadSafe<UsbDeviceHandle>;
  virtual ~UsbDeviceHandle();

 private:
  struct Transfer {
    Transfer();
    ~Transfer();

    UsbTransferType transfer_type;
    scoped_refptr<net::IOBuffer> buffer;
    size_t length;
    UsbTransferCallback callback;
  };

  // UsbDeviceHandle should only be created from UsbDevice class.
  UsbDeviceHandle(UsbService* service, int device, const uint16 vendor_id,
                  const uint16 product_id, PlatformUsbDeviceHandle handle);

  friend class UsbDevice;

  static void HandleTransferCompletionFileThread(
      PlatformUsbTransferHandle transfer);

  static void LIBUSB_CALL HandleTransferCompletion(
      PlatformUsbTransferHandle transfer);

  void TransferComplete(PlatformUsbTransferHandle transfer);

  // This only called from UsbDevice, thus always from FILE thread.
  void InternalClose();

  // Submits a transfer and starts tracking it. Retains the buffer and copies
  // the completion callback until the transfer finishes, whereupon it invokes
  // the callback then releases the buffer.
  void SubmitTransfer(PlatformUsbTransferHandle handle,
                      UsbTransferType transfer_type, net::IOBuffer* buffer,
                      const size_t length, const UsbTransferCallback& callback);

  // The UsbService isn't referenced here to prevent a dependency cycle between
  // the service and the devices. Since a service owns every device, and is
  // responsible for its destruction, there is no case where a UsbDevice can
  // have outlived its originating UsbService.
  UsbService* const service_;
  const int device_;
  const uint16 vendor_id_;
  const uint16 product_id_;

  PlatformUsbDeviceHandle handle_;

  // transfers_ tracks all in-flight transfers associated with this device,
  // allowing the device to retain the buffer and callback associated with a
  // transfer until such time that it completes.
  std::map<PlatformUsbTransferHandle, Transfer> transfers_;

  DISALLOW_COPY_AND_ASSIGN(UsbDeviceHandle);
};

#endif  // CHROME_BROWSER_USB_USB_DEVICE_HANDLE_H_
