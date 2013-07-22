// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef USB_TRANSFER_H_
#define USB_TRANSFER_H_

#include <vector>

#include "base/memory/ref_counted.h"
#include "chrome/browser/usb/usb_device.h"

class UsbTransfer : public base::RefCountedThreadSafe<UsbTransfer> {
 public:
  enum TransferRequestType { STANDARD, CLASS, VENDOR, RESERVED };
  enum TransferRecipient { DEVICE, INTERFACE, ENDPOINT, OTHER };

  bool is_submitted() const;
  PlatformUsbTransferHandle transfer_handle() const { return transfer_handle_; }

  void Abort();
  // Submit this transfer to the given device.
  // Upon a successful submission, The device will retain an reference of this
  // transfer.
  //
  // On UsbTransfer can only be submitted once.
  void Submit(
      scoped_refptr<UsbDevice> device,
      UsbTransferCallback callback);

  static scoped_refptr<UsbTransfer> CreateControlTransfer(
      const UsbEndpointDirection direction,
      const TransferRequestType request_type,
      const TransferRecipient recipient,
      const uint8 request,
      const uint16 value,
      const uint16 index,
      const scoped_refptr<net::IOBuffer> buffer,
      const size_t length,
      const unsigned int timeout);

  static scoped_refptr<UsbTransfer> CreateBulkTransfer(
      const UsbEndpointDirection direction,
      const uint8 endpoint,
      const scoped_refptr<net::IOBuffer> buffer,
      const size_t length,
      const unsigned int timeout);

  static scoped_refptr<UsbTransfer> CreateInterruptTransfer(
      const UsbEndpointDirection direction,
      const uint8 endpoint,
      const scoped_refptr<net::IOBuffer> buffer,
      const size_t length,
      const unsigned int timeout);

  static scoped_refptr<UsbTransfer> CreateIsochronousTransfer(
      const UsbEndpointDirection direction,
      const uint8 endpoint,
      const scoped_refptr<net::IOBuffer> buffer,
      const size_t length,
      const uint32_t num_iso_packets,
      const uint32_t packet_length,
      const unsigned int timeout);

 protected:
  DISALLOW_COPY_AND_ASSIGN(UsbTransfer);
  friend class base::RefCountedThreadSafe<UsbTransfer>;
  friend class UsbDevice;
  friend void API_CALL HandleTransferCompletion(
      PlatformUsbTransferHandle transfer);

  UsbTransfer();
  virtual ~UsbTransfer();

  void TransferCompleted();
  virtual void PostprocessData(size_t actual_length);

  static void HandleTransferCompletion(PlatformUsbTransferHandle handle);

  PlatformUsbTransferHandle transfer_handle_;
  bool is_submitted_;
  UsbTransferType transfer_type_;
  UsbTransferCallback callback_;
  scoped_refptr<net::IOBuffer> buffer_;
  size_t length_;
};

#endif  // USB_TRANSFER_H_
