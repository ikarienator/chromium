// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/usb/usb_device_handle.h"

#include <vector>

#include "base/stl_util.h"
#include "base/synchronization/lock.h"
#include "chrome/browser/usb/usb_interface.h"
#include "chrome/browser/usb/usb_service.h"
#include "content/public/browser/browser_thread.h"
#include "third_party/libusb/src/libusb/libusb.h"

using content::BrowserThread;

#define CHECK_DEVICE(callback, args...) \
  do { \
    if (handle_ == NULL) { \
      DVLOG(1) << "Device is disconnected: "; \
      callback.Run(args);\
      return; \
    } \
  } while (0)

#define CHECK_DEVICE_RETURN \
  do { \
    if (handle_ == NULL) { \
      DVLOG(1) << "Device is disconnected: "; \
      return; \
    } \
  } while (0)

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

// This function dispatches a completed transfer to its handle.
// It is called from UsbEventDispatcher using libusb_handle_events_timeout.
static void LIBUSB_CALL HandleTransferCompletion(
    struct libusb_transfer* transfer) {
  UsbDeviceHandle* const device =
      reinterpret_cast<UsbDeviceHandle*>(transfer->user_data);

  device->TransferComplete(transfer);
  libusb_free_transfer(transfer);
}

}  // namespace

UsbDeviceHandle::Transfer::Transfer()
    : transfer_type(USB_TRANSFER_CONTROL), length(0) {}

UsbDeviceHandle::Transfer::~Transfer() {}

UsbDeviceHandle::UsbDeviceHandle(UsbService* service,
                                 int device,
                                 PlatformUsbDeviceHandle handle)
    : service_(service), device_(device), handle_(handle) {
  DCHECK(handle) << "Cannot create device with NULL handle.";
}

UsbDeviceHandle::UsbDeviceHandle()
    : service_(NULL), device_(0), handle_(NULL) {
}

UsbDeviceHandle::~UsbDeviceHandle() {
  InternalClose();
}

void UsbDeviceHandle::Close(const base::Callback<void()>& callback) {
  if (handle_ == 0)
    return;
  BrowserThread::PostTask(
      BrowserThread::FILE,
      FROM_HERE,
      base::Bind(&UsbService::CloseDeviceHandle,
                 base::Unretained(service_),
                 make_scoped_refptr(this),
                 callback));
}

void UsbDeviceHandle::TransferComplete(PlatformUsbTransferHandle handle) {
  Transfer transfer;
  base::AutoLock handle_guard(handle_lock_);
  // If handle->user_data is cleared after we obtained it, the handles will be
  // removed and callbacks is already called in InternalClose. This case we can
  // simply return.
  if (handle->user_data == NULL)
    return;

  {
    base::AutoLock guard(transfer_lock_);
    transfer = transfers_[handle];
    transfers_.erase(handle);
  }

  CHECK_DEVICE(transfer.callback,
               USB_TRANSFER_DISCONNECT,
               scoped_refptr<net::IOBuffer>(), 0);

  if (handle->status != LIBUSB_TRANSFER_COMPLETED &&
      handle->status != LIBUSB_TRANSFER_CANCELLED) {
    service_->ScheduleEnumerateDevice();
  }

  DCHECK(handle->actual_length >= 0) << "Negative actual length received";
  size_t actual_length =
      static_cast<size_t>(std::max(handle->actual_length, 0));

  DCHECK(transfer.length >= actual_length) <<
      "data too big for our buffer (libusb failure?)";

  scoped_refptr<net::IOBuffer> buffer = transfer.buffer;
  switch (transfer.transfer_type) {
    case USB_TRANSFER_CONTROL:
      // If the transfer is a control transfer we do not expose the control
      // setup header to the caller. This logic strips off the header if
      // present before invoking the callback provided with the transfer.
      if (actual_length > 0) {
        CHECK(transfer.length >= LIBUSB_CONTROL_SETUP_SIZE) <<
            "buffer was not correctly set: too small for the control header";

        if (transfer.length >= actual_length &&
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
                    transfer.length);
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
  }

  transfer.callback.Run(ConvertTransferStatus(handle->status), buffer,
                        actual_length);
}

void UsbDeviceHandle::ListInterfaces(UsbConfigDescriptor* config,
                                     const UsbInterfaceCallback& callback) {
  CHECK_DEVICE(callback, false);

  PlatformUsbDevice device = libusb_get_device(handle_);

  PlatformUsbConfigDescriptor platform_config;
  const int list_result = libusb_get_active_config_descriptor(device,
      &platform_config);
  if (list_result == 0) {
    config->Reset(platform_config);
  }
  callback.Run(list_result == 0);
}

void UsbDeviceHandle::ClaimInterface(const int interface_number,
                                     const UsbInterfaceCallback& callback) {
  CHECK_DEVICE(callback, false);

  const int claim_result = libusb_claim_interface(handle_, interface_number);
  callback.Run(claim_result == 0);
}

void UsbDeviceHandle::ReleaseInterface(const int interface_number,
                                 const UsbInterfaceCallback& callback) {
  CHECK_DEVICE(callback, false);

  const int release_result = libusb_release_interface(handle_,
                                                      interface_number);
  callback.Run(release_result == 0);
}

void UsbDeviceHandle::SetInterfaceAlternateSetting(
    const int interface_number,
    const int alternate_setting,
    const UsbInterfaceCallback& callback) {
  CHECK_DEVICE(callback, false);

  const int setting_result = libusb_set_interface_alt_setting(handle_,
      interface_number, alternate_setting);

  callback.Run(setting_result == 0);
}

void UsbDeviceHandle::ControlTransfer(const UsbEndpointDirection direction,
    const TransferRequestType request_type, const TransferRecipient recipient,
    const uint8 request, const uint16 value, const uint16 index,
    net::IOBuffer* buffer, const size_t length, const unsigned int timeout,
    const UsbTransferCallback& callback) {
  CHECK_DEVICE(callback, USB_TRANSFER_DISCONNECT,
               scoped_refptr<net::IOBuffer>(), 0);

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
  CHECK_DEVICE(callback, USB_TRANSFER_DISCONNECT,
               scoped_refptr<net::IOBuffer>(), 0);

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
  CHECK_DEVICE(callback, USB_TRANSFER_DISCONNECT,
               scoped_refptr<net::IOBuffer>(), 0);

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
  CHECK_DEVICE(callback, USB_TRANSFER_DISCONNECT,
               scoped_refptr<net::IOBuffer>(), 0);

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

void UsbDeviceHandle::ResetDevice(const base::Callback<void(bool)>& callback) {
  // Blocking operation. Run it on the FILE thread.
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  CHECK_DEVICE(callback, false);
  callback.Run(libusb_reset_device(handle_) == 0);
}

void UsbDeviceHandle::InternalClose() {
  base::AutoLock handle_guard(handle_lock_);
  base::AutoLock guard(transfer_lock_);
  if (handle_ == NULL)
    return;

  // Cancel all the transfers.
  for (std::map<PlatformUsbTransferHandle, Transfer>::iterator it
      = transfers_.begin(); it != transfers_.end(); it++) {
    it->first->user_data = NULL;
    it->second.callback.Run(USB_TRANSFER_DISCONNECT,
                            scoped_refptr<net::IOBuffer>(), 0);
  }
  transfers_.clear();
  libusb_close(handle_);
  handle_ = NULL;
}

void UsbDeviceHandle::SubmitTransfer(PlatformUsbTransferHandle handle,
                                     UsbTransferType transfer_type,
                                     net::IOBuffer* buffer,
                                     const size_t length,
                                     const UsbTransferCallback& callback) {
  base::AutoLock lock(transfer_lock_);
  // This check must be done after the lock.
  if (!handle_)
    return;

  Transfer transfer;
  transfer.transfer_type = transfer_type;
  transfer.buffer = buffer;
  transfer.length = length;
  transfer.callback = callback;

  transfers_[handle] = transfer;
  libusb_submit_transfer(handle);
}
