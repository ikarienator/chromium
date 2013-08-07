// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/usb/usb_device_handle.h"

#include <algorithm>
#include <vector>

#include "base/stl_util.h"
#include "base/strings/string16.h"
#include "base/synchronization/lock.h"
#include "chrome/browser/usb/usb_device.h"
#include "chrome/browser/usb/usb_interface.h"
#include "chrome/browser/usb/usb_service.h"
#include "content/public/browser/browser_thread.h"
#include "third_party/libusb/src/libusb/libusb.h"

using content::BrowserThread;

namespace {

static uint8 ConvertTransferDirection(
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

static uint8 CreateRequestType(const UsbEndpointDirection direction,
    const UsbDeviceHandle::TransferRequestType request_type,
    const UsbDeviceHandle::TransferRecipient recipient) {
  uint8 result = ConvertTransferDirection(direction);

  switch (request_type) {
    case UsbDeviceHandle::STANDARD:
      result |= LIBUSB_REQUEST_TYPE_STANDARD;
      break;
    case UsbDeviceHandle::CLASS:
      result |= LIBUSB_REQUEST_TYPE_CLASS;
      break;
    case UsbDeviceHandle::VENDOR:
      result |= LIBUSB_REQUEST_TYPE_VENDOR;
      break;
    case UsbDeviceHandle::RESERVED:
      result |= LIBUSB_REQUEST_TYPE_RESERVED;
      break;
  }

  switch (recipient) {
    case UsbDeviceHandle::DEVICE:
      result |= LIBUSB_RECIPIENT_DEVICE;
      break;
    case UsbDeviceHandle::INTERFACE:
      result |= LIBUSB_RECIPIENT_INTERFACE;
      break;
    case UsbDeviceHandle::ENDPOINT:
      result |= LIBUSB_RECIPIENT_ENDPOINT;
      break;
    case UsbDeviceHandle::OTHER:
      result |= LIBUSB_RECIPIENT_OTHER;
      break;
  }

  return result;
}

static UsbTransferStatus ConvertTransferStatus(
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

static void LIBUSB_CALL HandleTransferCompletion(
    struct libusb_transfer* transfer) {
  UsbDeviceHandle* const device =
      reinterpret_cast<UsbDeviceHandle*>(transfer->user_data);
  device->TransferComplete(transfer);
}

}  // namespace

UsbDeviceHandle::Transfer::Transfer()
    : transfer_type(USB_TRANSFER_CONTROL),
      length(0) {
}

UsbDeviceHandle::Transfer::~Transfer() {}

UsbDeviceHandle::UsbDeviceHandle(
    UsbDevice* device,
    PlatformUsbDeviceHandle handle)
    : device_(device), handle_(handle) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(handle) << "Cannot create device with NULL handle.";
}

UsbDeviceHandle::UsbDeviceHandle() : device_(NULL), handle_(NULL) {
}

UsbDeviceHandle::~UsbDeviceHandle() {
  DCHECK(thread_checker_.CalledOnValidThread());
  Close();
}

scoped_refptr<UsbDevice> UsbDeviceHandle::device() const {
  return device_;
}

void UsbDeviceHandle::Close() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (handle_)
    device_->Close(this);
}

void UsbDeviceHandle::TransferComplete(PlatformUsbTransferHandle handle) {
  base::AutoLock lock(lock_);

  // TODO(gdk): Handle device disconnect.
  DCHECK(ContainsKey(transfers_, handle)) << "Missing transfer completed";
  Transfer* const transfer = &transfers_[handle];

  DCHECK_GE(handle->actual_length, 0) << "Negative actual length received";
  size_t actual_length =
      static_cast<size_t>(std::max(handle->actual_length, 0));

  DCHECK(transfer->length >= actual_length) <<
      "data too big for our buffer (libusb failure?)";

  scoped_refptr<net::IOBuffer> buffer = transfer->buffer;
  switch (transfer->transfer_type) {
    case USB_TRANSFER_CONTROL:
      // If the transfer is a control transfer we do not expose the control
      // setup header to the caller. This logic strips off the header if
      // present before invoking the callback provided with the transfer.
      if (actual_length > 0) {
        CHECK(transfer->length >= LIBUSB_CONTROL_SETUP_SIZE) <<
            "buffer was not correctly set: too small for the control header";

        if (transfer->length >= actual_length &&
            actual_length >= LIBUSB_CONTROL_SETUP_SIZE) {
          // If the payload is zero bytes long, pad out the allocated buffer
          // size to one byte so that an IOBuffer of that size can be allocated.
          scoped_refptr<net::IOBuffer> resized_buffer = new net::IOBuffer(
              std::max(actual_length, static_cast<size_t>(1)));
          memcpy(resized_buffer->data(),
                 buffer->data() + LIBUSB_CONTROL_SETUP_SIZE,
                 actual_length);
          buffer = resized_buffer;
        }
      }
      break;

    case USB_TRANSFER_ISOCHRONOUS:
      // Isochronous replies might carry data in the different isoc packets even
      // if the transfer actual_data value is zero. Furthermore, not all of the
      // received packets might contain data, so we need to calculate how many
      // data bytes we are effectively providing and pack the results.
      if (actual_length == 0) {
        size_t packet_buffer_start = 0;
        for (int i = 0; i < handle->num_iso_packets; ++i) {
          PlatformUsbIsoPacketDescriptor packet = &handle->iso_packet_desc[i];
          if (packet->actual_length > 0) {
            // We don't need to copy as long as all packets until now provide
            // all the data the packet can hold.
            if (actual_length < packet_buffer_start) {
              CHECK(packet_buffer_start + packet->actual_length <=
                    transfer->length);
              memmove(buffer->data() + actual_length,
                      buffer->data() + packet_buffer_start,
                      packet->actual_length);
            }
            actual_length += packet->actual_length;
          }

          packet_buffer_start += packet->length;
        }
      }
      break;

    case USB_TRANSFER_BULK:
    case USB_TRANSFER_INTERRUPT:
      break;

    default:
      NOTREACHED() << "Invalid usb transfer type";
      break;
  }

  transfer->callback.Run(ConvertTransferStatus(handle->status), buffer,
                         actual_length);

  transfers_.erase(handle);
  libusb_free_transfer(handle);
}

bool UsbDeviceHandle::ClaimInterface(const int interface_number) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!handle_) return false;

  const int claim_result = libusb_claim_interface(handle_, interface_number);
  return claim_result == 0;
}

bool UsbDeviceHandle::ReleaseInterface(const int interface_number) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!handle_) return false;

  const int release_result = libusb_release_interface(handle_,
                                                      interface_number);
  return release_result == 0;
}

bool UsbDeviceHandle::SetInterfaceAlternateSetting(
    const int interface_number,
    const int alternate_setting) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!handle_) return false;

  const int setting_result = libusb_set_interface_alt_setting(handle_,
      interface_number, alternate_setting);

  return setting_result == 0;
}

bool UsbDeviceHandle::ResetDevice() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!handle_) return false;

  return libusb_reset_device(handle_) == 0;
}

bool UsbDeviceHandle::GetSerial(base::string16* serial) {
  DCHECK(thread_checker_.CalledOnValidThread());
  PlatformUsbDevice device = libusb_get_device(handle_);
  libusb_device_descriptor desc;

  if (libusb_get_device_descriptor(device, &desc) != LIBUSB_SUCCESS)
    return false;

  if (desc.iSerialNumber == 0)
    return false;

  // Getting supported language ID.
  uint16 langid[128] = { 0 };

  int size = libusb_get_string_descriptor(
      handle_, 0, 0,
      reinterpret_cast<unsigned char*>(&langid[0]), sizeof(langid));
  if (size < 0)
    return false;

  int language_count = (size - 2) / 2;

  for (int i = 1; i <= language_count; ++i) {
    // Get the string using language ID.
    char16 text[256] = { 0 };
    size = libusb_get_string_descriptor(
        handle_, desc.iSerialNumber, langid[i],
        reinterpret_cast<unsigned char*>(&text[0]), sizeof(text));
    if (size <= 2)
      continue;
    if ((text[0] >> 8) != LIBUSB_DT_STRING)
      continue;
    if ((text[0] & 255) > size)
      continue;

    size = size / 2 - 1;
    *serial = base::string16(text + 1, size);
    return true;
  }
  return false;
}

void UsbDeviceHandle::ControlTransfer(const UsbEndpointDirection direction,
    const TransferRequestType request_type, const TransferRecipient recipient,
    const uint8 request, const uint16 value, const uint16 index,
    net::IOBuffer* buffer, const size_t length, const unsigned int timeout,
    const UsbTransferCallback& callback) {
  if (!handle_) {
    callback.Run(USB_TRANSFER_DISCONNECT, buffer, 0);
    return;
  }

  const size_t resized_length = LIBUSB_CONTROL_SETUP_SIZE + length;
  scoped_refptr<net::IOBuffer> resized_buffer(new net::IOBufferWithSize(
      resized_length));
  memcpy(resized_buffer->data() + LIBUSB_CONTROL_SETUP_SIZE, buffer->data(),
         length);

  struct libusb_transfer* const transfer = libusb_alloc_transfer(0);
  const uint8 converted_type = CreateRequestType(direction, request_type,
                                                 recipient);
  libusb_fill_control_setup(reinterpret_cast<uint8*>(resized_buffer->data()),
                            converted_type, request, value, index, length);
  libusb_fill_control_transfer(transfer, handle_, reinterpret_cast<uint8*>(
      resized_buffer->data()), HandleTransferCompletion, this, timeout);
  SubmitTransfer(transfer,
                 USB_TRANSFER_CONTROL,
                 resized_buffer.get(),
                 resized_length,
                 callback);
}

void UsbDeviceHandle::BulkTransfer(const UsbEndpointDirection direction,
    const uint8 endpoint, net::IOBuffer* buffer, const size_t length,
    const unsigned int timeout, const UsbTransferCallback& callback) {
  if (!handle_) {
    callback.Run(USB_TRANSFER_DISCONNECT, buffer, 0);
    return;
  }

  struct libusb_transfer* const transfer = libusb_alloc_transfer(0);
  const uint8 new_endpoint = ConvertTransferDirection(direction) | endpoint;
  libusb_fill_bulk_transfer(transfer, handle_, new_endpoint,
      reinterpret_cast<uint8*>(buffer->data()), length,
      HandleTransferCompletion, this, timeout);
  SubmitTransfer(transfer, USB_TRANSFER_BULK, buffer, length, callback);
}

void UsbDeviceHandle::InterruptTransfer(const UsbEndpointDirection direction,
    const uint8 endpoint, net::IOBuffer* buffer, const size_t length,
    const unsigned int timeout, const UsbTransferCallback& callback) {
  if (!handle_) {
    callback.Run(USB_TRANSFER_DISCONNECT, buffer, 0);
    return;
  }

  struct libusb_transfer* const transfer = libusb_alloc_transfer(0);
  const uint8 new_endpoint = ConvertTransferDirection(direction) | endpoint;
  libusb_fill_interrupt_transfer(transfer, handle_, new_endpoint,
      reinterpret_cast<uint8*>(buffer->data()), length,
      HandleTransferCompletion, this, timeout);
  SubmitTransfer(transfer, USB_TRANSFER_INTERRUPT, buffer, length, callback);
}

void UsbDeviceHandle::IsochronousTransfer(const UsbEndpointDirection direction,
    const uint8 endpoint, net::IOBuffer* buffer, const size_t length,
    const unsigned int packets, const unsigned int packet_length,
    const unsigned int timeout, const UsbTransferCallback& callback) {
  if (!handle_) {
    callback.Run(USB_TRANSFER_DISCONNECT, buffer, 0);
    return;
  }

  const uint64 total_length = packets * packet_length;
  CHECK(packets <= length && total_length <= length) <<
      "transfer length is too small";

  struct libusb_transfer* const transfer = libusb_alloc_transfer(packets);
  const uint8 new_endpoint = ConvertTransferDirection(direction) | endpoint;
  libusb_fill_iso_transfer(transfer, handle_, new_endpoint,
      reinterpret_cast<uint8*>(buffer->data()), length, packets,
      HandleTransferCompletion, this, timeout);
  libusb_set_iso_packet_lengths(transfer, packet_length);

  SubmitTransfer(transfer, USB_TRANSFER_ISOCHRONOUS, buffer, length, callback);
}

void UsbDeviceHandle::SubmitTransfer(PlatformUsbTransferHandle handle,
                               UsbTransferType transfer_type,
                               net::IOBuffer* buffer,
                               const size_t length,
                               const UsbTransferCallback& callback) {
  Transfer transfer;
  transfer.transfer_type = transfer_type;
  transfer.buffer = buffer;
  transfer.length = length;
  transfer.callback = callback;

  {
    base::AutoLock lock(lock_);
    transfers_[handle] = transfer;
  }
  if (libusb_submit_transfer(handle) != LIBUSB_SUCCESS) {
    base::AutoLock lock(lock_);
    transfers_.erase(handle);
    callback.Run(USB_TRANSFER_DISCONNECT, buffer, 0);
  }
}

void UsbDeviceHandle::InternalClose() {
  DCHECK(thread_checker_.CalledOnValidThread());
  libusb_close(handle_);
  handle_ = NULL;
  device_ = NULL;
}
