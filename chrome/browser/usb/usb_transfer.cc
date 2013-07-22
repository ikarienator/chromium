// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "usb_transfer.h"

#include "content/public/browser/browser_thread.h"
#include "third_party/libusb/src/libusb/libusb.h"

using content::BrowserThread;

namespace {

uint8 ConvertTransferDirection(
    const UsbEndpointDirection direction) {
  switch (direction) {
    case USB_DIRECTION_INBOUND:
      return LIBUSB_ENDPOINT_IN;
    case USB_DIRECTION_OUTBOUND:
      return LIBUSB_ENDPOINT_OUT;
    default:
      NOTREACHED();
      return LIBUSB_ENDPOINT_IN;
  }
}

uint8 CreateRequestType(
    const UsbEndpointDirection direction,
    const UsbTransfer::TransferRequestType request_type,
    const UsbTransfer::TransferRecipient recipient) {
  uint8 result = ConvertTransferDirection(direction);

  switch (request_type) {
    case UsbTransfer::STANDARD:
      result |= LIBUSB_REQUEST_TYPE_STANDARD;
      break;
    case UsbTransfer::CLASS:
      result |= LIBUSB_REQUEST_TYPE_CLASS;
      break;
    case UsbTransfer::VENDOR:
      result |= LIBUSB_REQUEST_TYPE_VENDOR;
      break;
    case UsbTransfer::RESERVED:
      result |= LIBUSB_REQUEST_TYPE_RESERVED;
      break;
  }

  switch (recipient) {
    case UsbTransfer::DEVICE:
      result |= LIBUSB_RECIPIENT_DEVICE;
      break;
    case UsbTransfer::INTERFACE:
      result |= LIBUSB_RECIPIENT_INTERFACE;
      break;
    case UsbTransfer::ENDPOINT:
      result |= LIBUSB_RECIPIENT_ENDPOINT;
      break;
    case UsbTransfer::OTHER:
      result |= LIBUSB_RECIPIENT_OTHER;
      break;
  }

  return result;
}

UsbTransferStatus ConvertTransferStatus(
    const libusb_transfer_status status) {
  switch (status) {
    case LIBUSB_TRANSFER_COMPLETED:
      return USB_TRANSFER_COMPLETED;
    case LIBUSB_TRANSFER_ERROR:
      return USB_TRANSFER_ERROR;
    case LIBUSB_TRANSFER_TIMED_OUT:
      return USB_TRANSFER_TIMEOUT;
    case LIBUSB_TRANSFER_STALL:
      return USB_TRANSFER_STALLED;
    case LIBUSB_TRANSFER_NO_DEVICE:
      return USB_TRANSFER_DISCONNECT;
    case LIBUSB_TRANSFER_OVERFLOW:
      return USB_TRANSFER_OVERFLOW;
    case LIBUSB_TRANSFER_CANCELLED:
      return USB_TRANSFER_CANCELLED;
    default:
      NOTREACHED();
      return USB_TRANSFER_ERROR;
  }
}

}  // namespace

void API_CALL HandleTransferCompletion(
    PlatformUsbTransferHandle transfer_handle) {
  BrowserThread::PostTask(
      BrowserThread::FILE,
      FROM_HERE,
      base::Bind(&UsbTransfer::HandleTransferCompletion, transfer_handle));
}

class UsbControlTransfer : public UsbTransfer {
 private:
  friend class UsbTransfer;
  scoped_refptr<net::IOBuffer> original_buffer_;
  UsbControlTransfer() {}
  virtual ~UsbControlTransfer() {}
  virtual void PostprocessData(size_t actual_length) OVERRIDE {
    size_t length = length_ + LIBUSB_CONTROL_SETUP_SIZE;
    // If the transfer is a control transfer we do not expose the control
    // setup header to the caller. This logic strips off the header if
    // present before invoking the callback provided with the
    if (length >= actual_length &&
        actual_length >= LIBUSB_CONTROL_SETUP_SIZE) {
      // If the payload is zero bytes long, pad out the allocated buffer
      // size to one byte so that an IOBuffer of that size can be allocated.
      memcpy(original_buffer_->data(),
             buffer_->data() + LIBUSB_CONTROL_SETUP_SIZE,
             actual_length);
      buffer_ = original_buffer_;
    }
  }
};

class UsbIsochronousTransfer : public UsbTransfer {
 private:
  friend class UsbTransfer;
  UsbIsochronousTransfer() {}
  virtual ~UsbIsochronousTransfer() {}
  virtual void PostprocessData(size_t actual_length) OVERRIDE {
    // Isochronous replies might carry data in the different isoc packets even
    // if the transfer actual_data value is zero. Furthermore, not all of the
    // received packets might contain data, so we need to calculate how many
    // data bytes we are effectively providing and pack the results.
    if (actual_length == 0) {
      size_t packet_buffer_start = 0;
      for (int i = 0; i < transfer_handle_->num_iso_packets; ++i) {
        PlatformUsbIsoPacketDescriptor packet =
            &transfer_handle_->iso_packet_desc[i];
        if (packet->actual_length > 0) {
          // We don't need to copy as long as all packets until now provide
          // all the data the packet can hold.
          if (actual_length < packet_buffer_start) {
            CHECK(packet_buffer_start + packet->actual_length <= length_);
            memmove(buffer_->data() + actual_length,
                    buffer_->data() + packet_buffer_start,
                    packet->actual_length);
          }
          actual_length += packet->actual_length;
        }

        packet_buffer_start += packet->length;
      }
    }
  }
};

void UsbTransfer::Abort() {
  transfer_handle_->user_data = NULL;
  libusb_cancel_transfer(transfer_handle_);
  TransferCompleted();
}

void UsbTransfer::Submit(
    scoped_refptr<UsbDevice> device,
    UsbTransferCallback callback) {
  callback_ = callback;
  transfer_handle_->dev_handle = device->handle();
  AddRef();
  if (0 != libusb_submit_transfer(transfer_handle_)) {
    transfer_handle_->status = LIBUSB_TRANSFER_ERROR;
    TransferCompleted();
  }
}

scoped_refptr<UsbTransfer> UsbTransfer::CreateControlTransfer(
    const UsbEndpointDirection direction,
    const TransferRequestType request_type,
    const TransferRecipient recipient,
    const uint8 request,
    const uint16 value,
    const uint16 index,
    const scoped_refptr<net::IOBuffer> buffer,
    const size_t length,
    const unsigned int timeout) {
  UsbControlTransfer* transfer = new UsbControlTransfer();
  transfer->transfer_handle_ = libusb_alloc_transfer(0);
  transfer->original_buffer_ = buffer;
  transfer->transfer_type_ = USB_TRANSFER_CONTROL;
  transfer->buffer_ = new net::IOBuffer(
      length + LIBUSB_CONTROL_SETUP_SIZE);
  transfer->length_ = length + LIBUSB_CONTROL_SETUP_SIZE;
  memcpy(transfer->buffer_->data() + LIBUSB_CONTROL_SETUP_SIZE,
    buffer->data(),
    length);

  libusb_fill_control_setup(
      reinterpret_cast<unsigned char*>(transfer->buffer_->data()),
      CreateRequestType(direction, request_type, recipient),
      request,
      value,
      index,
      length);
  libusb_fill_control_transfer(
      transfer->transfer_handle_,
      NULL,
      reinterpret_cast<unsigned char*>(transfer->buffer_->data()),
      HandleTransferCompletion,
      transfer,
      timeout);
  return transfer;
}

scoped_refptr<UsbTransfer> UsbTransfer::CreateBulkTransfer(
    const UsbEndpointDirection direction,
    const uint8 endpoint,
    const scoped_refptr<net::IOBuffer> buffer,
    const size_t length,
    const unsigned int timeout) {
  UsbTransfer* transfer = new UsbTransfer();
  transfer->transfer_handle_ = libusb_alloc_transfer(0);
  transfer->buffer_ = buffer;
  transfer->length_ = length;
  transfer->transfer_type_ = USB_TRANSFER_BULK;
  libusb_fill_bulk_transfer(
      transfer->transfer_handle_,
      NULL,
      ConvertTransferDirection(direction) | endpoint,
      reinterpret_cast<unsigned char*>(transfer->buffer_->data()),
      length,
      HandleTransferCompletion,
      transfer,
      timeout);
  return transfer;
}

scoped_refptr<UsbTransfer> UsbTransfer::CreateInterruptTransfer(
    const UsbEndpointDirection direction,
    const uint8 endpoint,
    const scoped_refptr<net::IOBuffer> buffer,
    const size_t length,
    const unsigned int timeout) {
  UsbTransfer* transfer = new UsbTransfer();
  transfer->transfer_handle_ = libusb_alloc_transfer(0);
  transfer->buffer_ = buffer;
  transfer->length_ = length;
  transfer->transfer_type_ = USB_TRANSFER_INTERRUPT;
  libusb_fill_interrupt_transfer(
      transfer->transfer_handle_,
      NULL,
      ConvertTransferDirection(direction) | endpoint,
      reinterpret_cast<unsigned char*>(transfer->buffer_->data()),
      length,
      HandleTransferCompletion,
      transfer,
      timeout);
  return transfer;
}

scoped_refptr<UsbTransfer> UsbTransfer::CreateIsochronousTransfer(
    const UsbEndpointDirection direction,
    const uint8 endpoint,
    const scoped_refptr<net::IOBuffer> buffer,
    const size_t length,
    const uint32_t num_iso_packets,
    const uint32_t packet_length,
    const unsigned int timeout) {
  UsbIsochronousTransfer* transfer = new UsbIsochronousTransfer();
  transfer->transfer_handle_ = libusb_alloc_transfer(num_iso_packets);
  transfer->buffer_ = buffer;
  transfer->length_ = length;
  transfer->transfer_type_ = USB_TRANSFER_ISOCHRONOUS;
  libusb_fill_iso_transfer(
      transfer->transfer_handle_,
      NULL,
      ConvertTransferDirection(direction) | endpoint,
      reinterpret_cast<unsigned char*>(transfer->buffer_->data()),
      length,
      num_iso_packets,
      HandleTransferCompletion,
      transfer,
      timeout);
  libusb_set_iso_packet_lengths(transfer->transfer_handle_, packet_length);
  return transfer;
}

UsbTransfer::UsbTransfer()
    : transfer_handle_(NULL),
      is_submitted_(false),
      transfer_type_(USB_TRANSFER_CONTROL),
      length_(0) {
}

UsbTransfer::~UsbTransfer() {
  if (transfer_handle_) {
    libusb_free_transfer(transfer_handle_);
  }
}

void UsbTransfer::TransferCompleted() {
  DCHECK_GE(transfer_handle_->actual_length, 0) <<
      "Negative actual length received";

  size_t actual_length = transfer_handle_->actual_length;

  DCHECK_GE(length_, actual_length) <<
      "data too big for our buffer (libusb failure?)";

  if (transfer_handle_->status == LIBUSB_TRANSFER_COMPLETED)
    PostprocessData(actual_length);

  callback_.Run(
      ConvertTransferStatus(transfer_handle_->status),
      buffer_,
      actual_length);
  Release();
}

void UsbTransfer::PostprocessData(size_t actual_length) {}

void UsbTransfer::HandleTransferCompletion(
    PlatformUsbTransferHandle transfer_handle) {
  UsbTransfer* const transfer =
      reinterpret_cast<UsbTransfer*>(transfer_handle->user_data);
  if (transfer)
    transfer->TransferCompleted();
}
