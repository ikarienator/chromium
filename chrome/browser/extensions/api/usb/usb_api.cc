// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/usb/usb_api.h"

#include <string>
#include <vector>

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/extensions/api/usb/usb_device_resource.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/usb/usb_service.h"
#include "chrome/browser/usb/usb_service_factory.h"
#include "chrome/common/extensions/api/usb.h"
#include "chrome/common/extensions/permissions/permissions_data.h"
#include "chrome/common/extensions/permissions/usb_device_permission.h"

namespace BulkTransfer = extensions::api::usb::BulkTransfer;
namespace ClaimInterface = extensions::api::usb::ClaimInterface;
namespace ListInterfaces = extensions::api::usb::ListInterfaces;
namespace CloseDevice = extensions::api::usb::CloseDevice;
namespace ControlTransfer = extensions::api::usb::ControlTransfer;
namespace FindDevices = extensions::api::usb::FindDevices;
namespace GetDevices = extensions::api::usb::GetDevices;
namespace OpenDevice = extensions::api::usb::OpenDevice;
namespace InterruptTransfer = extensions::api::usb::InterruptTransfer;
namespace IsochronousTransfer = extensions::api::usb::IsochronousTransfer;
namespace ReleaseInterface = extensions::api::usb::ReleaseInterface;
namespace ResetDevice = extensions::api::usb::ResetDevice;
namespace SetInterfaceAlternateSetting =
    extensions::api::usb::SetInterfaceAlternateSetting;
namespace usb = extensions::api::usb;

using content::BrowserThread;
using std::string;
using std::vector;
using usb::ControlTransferInfo;
using usb::Device;
using usb::DeviceHandle;
using usb::Direction;
using usb::EndpointDescriptor;
using usb::GenericTransferInfo;
using usb::InterfaceDescriptor;
using usb::IsochronousTransferInfo;
using usb::Recipient;
using usb::RequestType;
using usb::SynchronizationType;
using usb::TransferType;
using usb::UsageType;

namespace {

static const char kDataKey[] = "data";
static const char kResultCodeKey[] = "resultCode";

static const char kErrorCancelled[] = "Transfer was cancelled.";
static const char kErrorDisconnect[] = "Device disconnected.";
static const char kErrorGeneric[] = "Transfer failed.";
static const char kErrorOverflow[] = "Inbound transfer overflow.";
static const char kErrorStalled[] = "Transfer stalled.";
static const char kErrorTimeout[] = "Transfer timed out.";
static const char kErrorTransferLength[] = "Transfer length is insufficient.";

static const char kErrorCannotListInterfaces[] = "Error listing interfaces.";
static const char kErrorCannotClaimInterface[] = "Error claiming interface.";
static const char kErrorCannotReleaseInterface[] = "Error releasing interface.";
static const char kErrorCannotSetInterfaceAlternateSetting[] =
    "Error setting alternate interface setting.";
static const char kErrorConvertDirection[] = "Invalid transfer direction.";
static const char kErrorConvertRecipient[] = "Invalid transfer recipient.";
static const char kErrorConvertRequestType[] = "Invalid request type.";
static const char kErrorConvertSynchronizationType[] =
    "Invalid synchronization type";
static const char kErrorConvertTransferType[] = "Invalid endpoint type.";
static const char kErrorConvertUsageType[] = "Invalid usage type.";
static const char kErrorMalformedParameters[] = "Error parsing parameters.";
static const char kErrorNoDevice[] = "No such device.";
static const char kErrorPermissionDenied[] =
    "Permission to access device was denied";
static const char kErrorInvalidTransferLength[] =
    "Transfer length must be a positive number less than 104,857,600.";
static const char kErrorInvalidNumberOfPackets[] =
    "Number of packets must be a positive number less than 4,194,304.";
static const char kErrorInvalidPacketLength[] =
    "Packet length must be a positive number less than 65,536.";
static const char kErrorResetDevice[] =
    "Error resetting the device. The device has been closed.";

static const size_t kMaxTransferLength = 100 * 1024 * 1024;
static const int kMaxPackets = 4 * 1024 * 1024;
static const int kMaxPacketLength = 64 * 1024;

static UsbDeviceHandle* device_for_test_ = NULL;

static bool ConvertDirectionToApi(const UsbEndpointDirection& input,
                                  Direction* output) {
  switch (input) {
    case USB_DIRECTION_INBOUND:
      *output = usb::DIRECTION_IN;
      return true;
    case USB_DIRECTION_OUTBOUND:
      *output = usb::DIRECTION_OUT;
      return true;
    default:
      NOTREACHED();
      return false;
  }
}

static bool ConvertSynchronizationTypeToApi(
    const UsbSynchronizationType& input,
    extensions::api::usb::SynchronizationType* output) {
  switch (input) {
    case USB_SYNCHRONIZATION_NONE:
      *output = usb::SYNCHRONIZATION_TYPE_NONE;
      return true;
    case USB_SYNCHRONIZATION_ASYNCHRONOUS:
      *output = usb::SYNCHRONIZATION_TYPE_ASYNCHRONOUS;
      return true;
    case USB_SYNCHRONIZATION_ADAPTIVE:
      *output = usb::SYNCHRONIZATION_TYPE_ADAPTIVE;
      return true;
    case USB_SYNCHRONIZATION_SYNCHRONOUS:
      *output = usb::SYNCHRONIZATION_TYPE_SYNCHRONOUS;
      return true;
    default:
      NOTREACHED();
      return false;
  }
}

static bool ConvertTransferTypeToApi(
    const UsbTransferType& input,
    extensions::api::usb::TransferType* output) {
  switch (input) {
    case USB_TRANSFER_CONTROL:
      *output = usb::TRANSFER_TYPE_CONTROL;
      return true;
    case USB_TRANSFER_INTERRUPT:
      *output = usb::TRANSFER_TYPE_INTERRUPT;
      return true;
    case USB_TRANSFER_ISOCHRONOUS:
      *output = usb::TRANSFER_TYPE_ISOCHRONOUS;
      return true;
    case USB_TRANSFER_BULK:
      *output = usb::TRANSFER_TYPE_BULK;
      return true;
    default:
      NOTREACHED();
      return false;
  }
}

static bool ConvertUsageTypeToApi(const UsbUsageType& input,
                                  extensions::api::usb::UsageType* output) {
  switch (input) {
    case USB_USAGE_DATA:
      *output = usb::USAGE_TYPE_DATA;
      return true;
    case USB_USAGE_FEEDBACK:
      *output = usb::USAGE_TYPE_FEEDBACK;
      return true;
    case USB_USAGE_EXPLICIT_FEEDBACK:
      *output = usb::USAGE_TYPE_EXPLICITFEEDBACK;
      return true;
    default:
      NOTREACHED();
      return false;
  }
}

static bool ConvertDirection(const Direction& input,
                             UsbEndpointDirection* output) {
  switch (input) {
    case usb::DIRECTION_IN:
      *output = USB_DIRECTION_INBOUND;
      return true;
    case usb::DIRECTION_OUT:
      *output = USB_DIRECTION_OUTBOUND;
      return true;
    default:
      NOTREACHED();
      return false;
  }
}

static bool ConvertRequestType(const RequestType& input,
                               UsbDeviceHandle::TransferRequestType* output) {
  switch (input) {
    case usb::REQUEST_TYPE_STANDARD:
      *output = UsbDeviceHandle::STANDARD;
      return true;
    case usb::REQUEST_TYPE_CLASS:
      *output = UsbDeviceHandle::CLASS;
      return true;
    case usb::REQUEST_TYPE_VENDOR:
      *output = UsbDeviceHandle::VENDOR;
      return true;
    case usb::REQUEST_TYPE_RESERVED:
      *output = UsbDeviceHandle::RESERVED;
      return true;
    default:
      NOTREACHED();
      return false;
  }
}

static bool ConvertRecipient(const Recipient& input,
                             UsbDeviceHandle::TransferRecipient* output) {
  switch (input) {
    case usb::RECIPIENT_DEVICE:
      *output = UsbDeviceHandle::DEVICE;
      return true;
    case usb::RECIPIENT_INTERFACE:
      *output = UsbDeviceHandle::INTERFACE;
      return true;
    case usb::RECIPIENT_ENDPOINT:
      *output = UsbDeviceHandle::ENDPOINT;
      return true;
    case usb::RECIPIENT_OTHER:
      *output = UsbDeviceHandle::OTHER;
      return true;
    default:
      NOTREACHED();
      return false;
  }
}

template <class T>
static bool GetTransferSize(const T& input, size_t* output) {
  if (input.direction == usb::DIRECTION_IN) {
    const int* length = input.length.get();
    if (length && *length >= 0 &&
        static_cast<size_t>(*length) < kMaxTransferLength) {
      *output = *length;
      return true;
    }
  } else if (input.direction == usb::DIRECTION_OUT) {
    if (input.data.get()) {
      *output = input.data->size();
      return true;
    }
  }
  return false;
}

template <class T>
static scoped_refptr<net::IOBuffer> CreateBufferForTransfer(
    const T& input, UsbEndpointDirection direction, size_t size) {

  if (size >= kMaxTransferLength)
    return NULL;

  // Allocate a |size|-bytes buffer, or a one-byte buffer if |size| is 0. This
  // is due to an impedance mismatch between IOBuffer and URBs. An IOBuffer
  // cannot represent a zero-length buffer, while an URB can.
  scoped_refptr<net::IOBuffer> buffer =
      new net::IOBuffer(std::max(static_cast<size_t>(1), size));

  if (direction == USB_DIRECTION_INBOUND) {
    return buffer;
  } else if (direction == USB_DIRECTION_OUTBOUND) {
    if (input.data.get() && size <= input.data->size()) {
      memcpy(buffer->data(), input.data->data(), size);
      return buffer;
    }
  }
  NOTREACHED();
  return NULL;
}

static const char* ConvertTransferStatusToErrorString(
    const UsbTransferStatus status) {
  switch (status) {
    case USB_TRANSFER_COMPLETED:
      return "";
    case USB_TRANSFER_ERROR:
      return kErrorGeneric;
    case USB_TRANSFER_TIMEOUT:
      return kErrorTimeout;
    case USB_TRANSFER_CANCELLED:
      return kErrorCancelled;
    case USB_TRANSFER_STALLED:
      return kErrorStalled;
    case USB_TRANSFER_DISCONNECT:
      return kErrorDisconnect;
    case USB_TRANSFER_OVERFLOW:
      return kErrorOverflow;
    case USB_TRANSFER_LENGTH_SHORT:
      return kErrorTransferLength;
    default:
      NOTREACHED();
      return "";
  }
}

static base::DictionaryValue* CreateTransferInfo(
    UsbTransferStatus status,
    scoped_refptr<net::IOBuffer> data,
    size_t length) {
  base::DictionaryValue* result = new base::DictionaryValue();
  result->SetInteger(kResultCodeKey, status);
  if (data.get()) {
    result->Set(
        kDataKey,
        base::BinaryValue::CreateWithCopiedBuffer(data->data(), length));
  }
  return result;
}

static base::Value* PopulateDevice(int device_id, int vendor_id,
                                   int product_id) {
  Device device;
  device.device = device_id;
  device.vendor_id = vendor_id;
  device.product_id = product_id;
  return device.ToValue().release();
}

static base::Value* PopulateDeviceHandle(int handle, int vendor_id,
                                         int product_id) {
  DeviceHandle device_handle;
  device_handle.handle = handle;
  device_handle.vendor_id = vendor_id;
  device_handle.product_id = product_id;
  return device_handle.ToValue().release();
}

static base::Value* PopulateInterfaceDescriptor(
    int interface_number, int alternate_setting, int interface_class,
    int interface_subclass, int interface_protocol,
    std::vector<linked_ptr<EndpointDescriptor> >* endpoints) {
  InterfaceDescriptor descriptor;
  descriptor.interface_number = interface_number;
  descriptor.alternate_setting = alternate_setting;
  descriptor.interface_class = interface_class;
  descriptor.interface_subclass = interface_subclass;
  descriptor.interface_protocol = interface_subclass;
  descriptor.endpoints = *endpoints;
  return descriptor.ToValue().release();
}

}  // namespace

namespace extensions {

UsbAsyncApiFunction::UsbAsyncApiFunction() : manager_(NULL) {}

UsbAsyncApiFunction::~UsbAsyncApiFunction() {}

bool UsbAsyncApiFunction::PrePrepare() {
  manager_ = ApiResourceManager<UsbDeviceResource>::Get(profile());
  return manager_ != NULL;
}

bool UsbAsyncApiFunction::Respond() { return error_.empty(); }

UsbDeviceResource* UsbAsyncApiFunction::GetUsbDeviceResource(
    int api_resource_id) {
  UsbDeviceResource* resource =
      manager_->Get(extension_->id(), api_resource_id);

  if (resource == NULL) return NULL;

  if (device_for_test_) {
    return resource;
  }

  return resource;
}

void UsbAsyncApiFunction::RemoveUsbDeviceResource(int api_resource_id) {
  manager_->Remove(extension_->id(), api_resource_id);
}

void UsbAsyncApiFunction::CompleteWithError(const std::string& error) {
  SetError(error);
  AsyncWorkCompleted();
}

UsbAsyncApiTransferFunction::UsbAsyncApiTransferFunction() {}

UsbAsyncApiTransferFunction::~UsbAsyncApiTransferFunction() {}

void UsbAsyncApiTransferFunction::OnCompleted(UsbTransferStatus status,
                                              scoped_refptr<net::IOBuffer> data,
                                              size_t length) {
  if (status != USB_TRANSFER_COMPLETED)
    SetError(ConvertTransferStatusToErrorString(status));

  SetResult(CreateTransferInfo(status, data, length));
  AsyncWorkCompleted();
}

bool UsbAsyncApiTransferFunction::ConvertDirectionSafely(
    const Direction& input, UsbEndpointDirection* output) {
  const bool converted = ConvertDirection(input, output);
  if (!converted) SetError(kErrorConvertDirection);
  return converted;
}

bool UsbAsyncApiTransferFunction::ConvertRequestTypeSafely(
    const RequestType& input, UsbDeviceHandle::TransferRequestType* output) {
  const bool converted = ConvertRequestType(input, output);
  if (!converted) SetError(kErrorConvertRequestType);
  return converted;
}

bool UsbAsyncApiTransferFunction::ConvertRecipientSafely(
    const Recipient& input, UsbDeviceHandle::TransferRecipient* output) {
  const bool converted = ConvertRecipient(input, output);
  if (!converted) SetError(kErrorConvertRecipient);
  return converted;
}

UsbGetDevicesFunction::UsbGetDevicesFunction()
    : vendor_id_(0),
      product_id_(0),
      interface_id_(UsbDevicePermissionData::ANY_INTERFACE),
      service_(NULL) {}

UsbGetDevicesFunction::~UsbGetDevicesFunction() {}

void UsbGetDevicesFunction::SetDeviceForTest(UsbDeviceHandle* device) {
  device_for_test_ = device;
}

bool UsbGetDevicesFunction::PrePrepare() {
  if (device_for_test_) return UsbAsyncApiFunction::PrePrepare();
  service_ = UsbServiceFactory::GetInstance()->GetForProfile(profile());
  if (service_ == NULL) {
    LOG(WARNING) << "Could not get UsbService for active profile.";
    SetError(kErrorNoDevice);
    return false;
  }
  return UsbAsyncApiFunction::PrePrepare();
}

bool UsbGetDevicesFunction::Prepare() {
  scoped_ptr<GetDevices::Params> parameters =
      GetDevices::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(parameters.get());
  vendor_id_ = parameters->options.vendor_id;
  product_id_ = parameters->options.product_id;
  if (parameters->options.interface_id.get())
    interface_id_ = *parameters->options.interface_id;
  return true;
}

void UsbGetDevicesFunction::AsyncWorkStart() {
  result_.reset(new base::ListValue());

  if (device_for_test_) {
    result_->Append(PopulateDevice(device_for_test_->device(), 0, 0));
    SetResult(result_.release());
    AsyncWorkCompleted();
    return;
  }

  UsbDevicePermission::CheckParam param(vendor_id_, product_id_, interface_id_);
  if (!PermissionsData::CheckAPIPermissionWithParam(
          GetExtension(), APIPermission::kUsbDevice, &param)) {
    LOG(WARNING) << "Insufficient permissions to access device.";
    CompleteWithError(kErrorPermissionDenied);
    return;
  }

  BrowserThread::PostTask(
      BrowserThread::FILE,
      FROM_HERE,
      base::Bind(&UsbService::FindDevices, base::Unretained(service_),
                 vendor_id_, product_id_, interface_id_, &devices_,
                 base::Bind(&UsbGetDevicesFunction::OnDevicesFound, this)));
}

void UsbGetDevicesFunction::OnDevicesFound() {
  // Redirect this to virtual method.
  OnCompleted();
}

void UsbGetDevicesFunction::OnCompleted() {
  for (size_t i = 0; i < devices_.size(); ++i) {
    result_->Append(PopulateDevice(devices_[i], vendor_id_, product_id_));
  }

  SetResult(result_.release());
  AsyncWorkCompleted();
}

UsbFindDevicesFunction::UsbFindDevicesFunction() : UsbGetDevicesFunction() {}

UsbFindDevicesFunction::~UsbFindDevicesFunction() {}

bool UsbFindDevicesFunction::Prepare() {
  scoped_ptr<FindDevices::Params> parameters =
      FindDevices::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(parameters.get());
  vendor_id_ = parameters->options.vendor_id;
  product_id_ = parameters->options.product_id;
  if (parameters->options.interface_id.get())
    interface_id_ = *parameters->options.interface_id;
  return true;
}

void UsbFindDevicesFunction::OnCompleted() {
  for (size_t i = 0; i < devices_.size(); ++i) {
    scoped_refptr<UsbDeviceHandle> handle = service_->OpenDevice(devices_[i]);
    if (handle.get()) handles_.push_back(handle);
  }
  BrowserThread::PostTask(
      BrowserThread::IO,
      FROM_HERE,
      base::Bind(&UsbFindDevicesFunction::OpenDevices, this));
}

void UsbFindDevicesFunction::OpenDevices() {
  for (size_t i = 0; i < handles_.size(); ++i) {
    UsbDeviceResource* const resource =
        new UsbDeviceResource(extension_->id(), handles_[i]);
    result_->Append(
        PopulateDeviceHandle(manager_->Add(resource), handles_[i]->vendor_id(),
                             handles_[i]->product_id()));
  }
  SetResult(result_.release());
  AsyncWorkCompleted();
}

UsbOpenDeviceFunction::UsbOpenDeviceFunction() : service_(NULL) {}

UsbOpenDeviceFunction::~UsbOpenDeviceFunction() {}

bool UsbOpenDeviceFunction::PrePrepare() {
  if (device_for_test_) return UsbAsyncApiFunction::PrePrepare();
  service_ = UsbServiceFactory::GetInstance()->GetForProfile(profile());
  if (service_ == NULL) {
    LOG(WARNING) << "Could not get UsbService for active profile.";
    SetError(kErrorNoDevice);
    return false;
  }
  return UsbAsyncApiFunction::PrePrepare();
}

bool UsbOpenDeviceFunction::Prepare() {
  parameters_ = OpenDevice::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(parameters_.get());
  return true;
}

void UsbOpenDeviceFunction::AsyncWorkStart() {
  if (device_for_test_) {
    UsbDeviceResource* const resource =
        new UsbDeviceResource(extension_->id(), device_for_test_);
    SetResult(PopulateDeviceHandle(manager_->Add(resource),
                                   device_for_test_->vendor_id(),
                                   device_for_test_->product_id()));
    AsyncWorkCompleted();
    return;
  }
  BrowserThread::PostTask(BrowserThread::FILE, FROM_HERE,
                          base::Bind(&UsbOpenDeviceFunction::OpenDevice, this));
}

void UsbOpenDeviceFunction::OpenDevice() {
  scoped_refptr<UsbDeviceHandle> handle =
      service_->OpenDevice(parameters_->device.device);
  if (!handle.get()) {
    CompleteWithError(kErrorDisconnect);
    return;
  }
  // Pass to IO thread to use api resource manager.
  BrowserThread::PostTask(
      BrowserThread::IO,
      FROM_HERE,
      base::Bind(&UsbOpenDeviceFunction::OnCompleted, this, handle));
}

void UsbOpenDeviceFunction::OnCompleted(scoped_refptr<UsbDeviceHandle> handle) {
  UsbDeviceResource* const resource =
      new UsbDeviceResource(extension_->id(), handle);
  SetResult(PopulateDeviceHandle(manager_->Add(resource), handle->vendor_id(),
                                 handle->product_id()));
  AsyncWorkCompleted();
  return;
}

UsbListInterfacesFunction::UsbListInterfacesFunction() {}

UsbListInterfacesFunction::~UsbListInterfacesFunction() {}

bool UsbListInterfacesFunction::Prepare() {
  parameters_ = ListInterfaces::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(parameters_.get());
  return true;
}

void UsbListInterfacesFunction::AsyncWorkStart() {
  UsbDeviceResource* const resource =
      GetUsbDeviceResource(parameters_->handle.handle);
  if (!resource) {
    CompleteWithError(kErrorNoDevice);
    return;
  }

  config_ = new UsbConfigDescriptor();
  resource->ListInterfaces(
      config_.get(), base::Bind(&UsbListInterfacesFunction::OnCompleted, this));
}

void UsbListInterfacesFunction::OnCompleted(bool success) {
  if (!success) {
    SetError(kErrorCannotListInterfaces);
    AsyncWorkCompleted();
    return;
  }

  result_.reset(new base::ListValue());

  for (size_t i = 0, numInterfaces = config_->GetNumInterfaces();
       i < numInterfaces; ++i) {
    scoped_refptr<const UsbInterface> usbInterface(config_->GetInterface(i));
    for (size_t j = 0, numDescriptors = usbInterface->GetNumAltSettings();
         j < numDescriptors; ++j) {
      scoped_refptr<const UsbInterfaceDescriptor> descriptor =
          usbInterface->GetAltSetting(j);
      std::vector<linked_ptr<EndpointDescriptor> > endpoints;
      for (size_t k = 0, numEndpoints = descriptor->GetNumEndpoints();
           k < numEndpoints; ++k) {
        scoped_refptr<const UsbEndpointDescriptor> endpoint =
            descriptor->GetEndpoint(k);
        linked_ptr<EndpointDescriptor> endpoint_desc(new EndpointDescriptor());

        TransferType type;
        Direction direction;
        SynchronizationType synchronization;
        UsageType usage;

        if (!ConvertTransferTypeSafely(endpoint->GetTransferType(), &type) ||
            !ConvertDirectionSafely(endpoint->GetDirection(), &direction) ||
            !ConvertSynchronizationTypeSafely(
                endpoint->GetSynchronizationType(), &synchronization) ||
            !ConvertUsageTypeSafely(endpoint->GetUsageType(), &usage)) {
          SetError(kErrorCannotListInterfaces);
          AsyncWorkCompleted();
          return;
        }

        endpoint_desc->address = endpoint->GetAddress();
        endpoint_desc->type = type;
        endpoint_desc->direction = direction;
        endpoint_desc->maximum_packet_size = endpoint->GetMaximumPacketSize();
        endpoint_desc->synchronization = synchronization;
        endpoint_desc->usage = usage;

        int* polling_interval = new int;
        endpoint_desc->polling_interval.reset(polling_interval);
        *polling_interval = endpoint->GetPollingInterval();

        endpoints.push_back(endpoint_desc);
      }

      result_->Append(PopulateInterfaceDescriptor(
          descriptor->GetInterfaceNumber(),
          descriptor->GetAlternateSetting(),
          descriptor->GetInterfaceClass(),
          descriptor->GetInterfaceSubclass(),
          descriptor->GetInterfaceProtocol(),
          &endpoints));
    }
  }

  SetResult(result_.release());
  AsyncWorkCompleted();
}

bool UsbListInterfacesFunction::ConvertDirectionSafely(
    const UsbEndpointDirection& input,
    extensions::api::usb::Direction* output) {
  const bool converted = ConvertDirectionToApi(input, output);
  if (!converted)
    SetError(kErrorConvertDirection);
  return converted;
}

bool UsbListInterfacesFunction::ConvertSynchronizationTypeSafely(
    const UsbSynchronizationType& input,
    extensions::api::usb::SynchronizationType* output) {
  const bool converted = ConvertSynchronizationTypeToApi(input, output);
  if (!converted)
    SetError(kErrorConvertSynchronizationType);
  return converted;
}

bool UsbListInterfacesFunction::ConvertTransferTypeSafely(
    const UsbTransferType& input,
    extensions::api::usb::TransferType* output) {
  const bool converted = ConvertTransferTypeToApi(input, output);
  if (!converted)
    SetError(kErrorConvertTransferType);
  return converted;
}

bool UsbListInterfacesFunction::ConvertUsageTypeSafely(
    const UsbUsageType& input,
    extensions::api::usb::UsageType* output) {
  const bool converted = ConvertUsageTypeToApi(input, output);
  if (!converted)
    SetError(kErrorConvertUsageType);
  return converted;
}

UsbCloseDeviceFunction::UsbCloseDeviceFunction() {}

UsbCloseDeviceFunction::~UsbCloseDeviceFunction() {}

bool UsbCloseDeviceFunction::Prepare() {
  parameters_ = CloseDevice::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(parameters_.get());
  return true;
}

void UsbCloseDeviceFunction::AsyncWorkStart() {
  UsbDeviceResource* const resource =
      GetUsbDeviceResource(parameters_->handle.handle);
  if (!resource) {
    CompleteWithError(kErrorNoDevice);
    return;
  }

  resource->Close(base::Bind(&UsbCloseDeviceFunction::OnCompleted, this));
  RemoveUsbDeviceResource(parameters_->handle.handle);
}

void UsbCloseDeviceFunction::OnCompleted() { AsyncWorkCompleted(); }

UsbClaimInterfaceFunction::UsbClaimInterfaceFunction() {}

UsbClaimInterfaceFunction::~UsbClaimInterfaceFunction() {}

bool UsbClaimInterfaceFunction::Prepare() {
  parameters_ = ClaimInterface::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(parameters_.get());
  return true;
}

void UsbClaimInterfaceFunction::AsyncWorkStart() {
  UsbDeviceResource* resource =
      GetUsbDeviceResource(parameters_->handle.handle);
  if (!resource) {
    CompleteWithError(kErrorNoDevice);
    return;
  }

  resource->ClaimInterface(
      parameters_->interface_number,
      base::Bind(&UsbClaimInterfaceFunction::OnCompleted, this));
}

void UsbClaimInterfaceFunction::OnCompleted(bool success) {
  if (!success)
    SetError(kErrorCannotClaimInterface);
  AsyncWorkCompleted();
}

UsbReleaseInterfaceFunction::UsbReleaseInterfaceFunction() {}

UsbReleaseInterfaceFunction::~UsbReleaseInterfaceFunction() {}

bool UsbReleaseInterfaceFunction::Prepare() {
  parameters_ = ReleaseInterface::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(parameters_.get());
  return true;
}

void UsbReleaseInterfaceFunction::AsyncWorkStart() {
  UsbDeviceResource* resource =
      GetUsbDeviceResource(parameters_->handle.handle);
  if (!resource) {
    CompleteWithError(kErrorNoDevice);
    return;
  }

  resource->ReleaseInterface(
      parameters_->interface_number,
      base::Bind(&UsbReleaseInterfaceFunction::OnCompleted, this));
}

void UsbReleaseInterfaceFunction::OnCompleted(bool success) {
  if (!success)
    SetError(kErrorCannotReleaseInterface);
  AsyncWorkCompleted();
}

UsbSetInterfaceAlternateSettingFunction::
    UsbSetInterfaceAlternateSettingFunction() {}

UsbSetInterfaceAlternateSettingFunction::
    ~UsbSetInterfaceAlternateSettingFunction() {}

bool UsbSetInterfaceAlternateSettingFunction::Prepare() {
  parameters_ = SetInterfaceAlternateSetting::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(parameters_.get());
  return true;
}

void UsbSetInterfaceAlternateSettingFunction::AsyncWorkStart() {
  UsbDeviceResource* resource =
      GetUsbDeviceResource(parameters_->handle.handle);
  if (!resource) {
    CompleteWithError(kErrorNoDevice);
    return;
  }

  resource->SetInterfaceAlternateSetting(
      parameters_->interface_number, parameters_->alternate_setting,
      base::Bind(&UsbSetInterfaceAlternateSettingFunction::OnCompleted, this));
}

void UsbSetInterfaceAlternateSettingFunction::OnCompleted(bool success) {
  if (!success)
    SetError(kErrorCannotSetInterfaceAlternateSetting);
  AsyncWorkCompleted();
}

UsbControlTransferFunction::UsbControlTransferFunction() {}

UsbControlTransferFunction::~UsbControlTransferFunction() {}

bool UsbControlTransferFunction::Prepare() {
  parameters_ = ControlTransfer::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(parameters_.get());
  return true;
}

void UsbControlTransferFunction::AsyncWorkStart() {
  UsbDeviceResource* const resource =
      GetUsbDeviceResource(parameters_->handle.handle);
  if (!resource) {
    CompleteWithError(kErrorNoDevice);
    return;
  }

  const ControlTransferInfo& transfer = parameters_->transfer_info;

  UsbEndpointDirection direction;
  UsbDeviceHandle::TransferRequestType request_type;
  UsbDeviceHandle::TransferRecipient recipient;
  size_t size = 0;

  if (!ConvertDirectionSafely(transfer.direction, &direction) ||
      !ConvertRequestTypeSafely(transfer.request_type, &request_type) ||
      !ConvertRecipientSafely(transfer.recipient, &recipient)) {
    AsyncWorkCompleted();
    return;
  }

  if (!GetTransferSize(transfer, &size)) {
    CompleteWithError(kErrorInvalidTransferLength);
    return;
  }

  scoped_refptr<net::IOBuffer> buffer =
      CreateBufferForTransfer(transfer, direction, size);
  if (!buffer.get()) {
    CompleteWithError(kErrorMalformedParameters);
    return;
  }

  resource->ControlTransfer(
      direction, request_type, recipient, transfer.request, transfer.value,
      transfer.index, buffer.get(), size, 0,
      base::Bind(&UsbControlTransferFunction::OnCompleted, this));
}

UsbBulkTransferFunction::UsbBulkTransferFunction() {}

UsbBulkTransferFunction::~UsbBulkTransferFunction() {}

bool UsbBulkTransferFunction::Prepare() {
  parameters_ = BulkTransfer::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(parameters_.get());
  return true;
}

void UsbBulkTransferFunction::AsyncWorkStart() {
  UsbDeviceResource* const resource =
      GetUsbDeviceResource(parameters_->handle.handle);
  if (!resource) {
    CompleteWithError(kErrorNoDevice);
    return;
  }

  const GenericTransferInfo& transfer = parameters_->transfer_info;

  UsbEndpointDirection direction;
  size_t size = 0;

  if (!ConvertDirectionSafely(transfer.direction, &direction)) {
    AsyncWorkCompleted();
    return;
  }

  if (!GetTransferSize(transfer, &size)) {
    CompleteWithError(kErrorInvalidTransferLength);
    return;
  }

  scoped_refptr<net::IOBuffer> buffer =
      CreateBufferForTransfer(transfer, direction, size);
  if (!buffer.get()) {
    CompleteWithError(kErrorMalformedParameters);
    return;
  }

  resource->BulkTransfer(
      direction, transfer.endpoint, buffer.get(), size, 0,
      base::Bind(&UsbBulkTransferFunction::OnCompleted, this));
}

UsbInterruptTransferFunction::UsbInterruptTransferFunction() {}

UsbInterruptTransferFunction::~UsbInterruptTransferFunction() {}

bool UsbInterruptTransferFunction::Prepare() {
  parameters_ = InterruptTransfer::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(parameters_.get());
  return true;
}

void UsbInterruptTransferFunction::AsyncWorkStart() {
  UsbDeviceResource* const resource =
      GetUsbDeviceResource(parameters_->handle.handle);
  if (!resource) {
    CompleteWithError(kErrorNoDevice);
    return;
  }

  const GenericTransferInfo& transfer = parameters_->transfer_info;

  UsbEndpointDirection direction;
  size_t size = 0;

  if (!ConvertDirectionSafely(transfer.direction, &direction)) {
    AsyncWorkCompleted();
    return;
  }

  if (!GetTransferSize(transfer, &size)) {
    CompleteWithError(kErrorInvalidTransferLength);
    return;
  }

  scoped_refptr<net::IOBuffer> buffer =
      CreateBufferForTransfer(transfer, direction, size);
  if (!buffer.get()) {
    CompleteWithError(kErrorMalformedParameters);
    return;
  }

  resource->InterruptTransfer(
      direction,
      transfer.endpoint,
      buffer.get(),
      size,
      0,
      base::Bind(&UsbInterruptTransferFunction::OnCompleted, this));
}

UsbIsochronousTransferFunction::UsbIsochronousTransferFunction() {}

UsbIsochronousTransferFunction::~UsbIsochronousTransferFunction() {}

bool UsbIsochronousTransferFunction::Prepare() {
  parameters_ = IsochronousTransfer::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(parameters_.get());
  return true;
}

void UsbIsochronousTransferFunction::AsyncWorkStart() {
  UsbDeviceResource* const resource =
      GetUsbDeviceResource(parameters_->handle.handle);
  if (!resource) {
    CompleteWithError(kErrorNoDevice);
    return;
  }

  const IsochronousTransferInfo& transfer = parameters_->transfer_info;
  const GenericTransferInfo& generic_transfer = transfer.transfer_info;

  size_t size = 0;
  UsbEndpointDirection direction;

  if (!ConvertDirectionSafely(generic_transfer.direction, &direction)) {
    AsyncWorkCompleted();
    return;
  }
  if (!GetTransferSize(generic_transfer, &size)) {
    CompleteWithError(kErrorInvalidTransferLength);
    return;
  }
  if (transfer.packets < 0 || transfer.packets >= kMaxPackets) {
    CompleteWithError(kErrorInvalidNumberOfPackets);
    return;
  }
  unsigned int packets = transfer.packets;
  if (transfer.packet_length < 0 ||
      transfer.packet_length >= kMaxPacketLength) {
    CompleteWithError(kErrorInvalidPacketLength);
    return;
  }
  unsigned int packet_length = transfer.packet_length;
  const uint64 total_length = packets * packet_length;
  if (packets > size || total_length > size) {
    CompleteWithError(kErrorTransferLength);
    return;
  }

  scoped_refptr<net::IOBuffer> buffer =
      CreateBufferForTransfer(generic_transfer, direction, size);
  if (!buffer.get()) {
    CompleteWithError(kErrorMalformedParameters);
    return;
  }

  resource->IsochronousTransfer(
      direction,
      generic_transfer.endpoint,
      buffer.get(),
      size,
      packets,
      packet_length,
      0,
      base::Bind(&UsbIsochronousTransferFunction::OnCompleted, this));
}

UsbResetDeviceFunction::UsbResetDeviceFunction() {}

UsbResetDeviceFunction::~UsbResetDeviceFunction() {}

bool UsbResetDeviceFunction::Prepare() {
  parameters_ = ResetDevice::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(parameters_.get());
  return true;
}

void UsbResetDeviceFunction::AsyncWorkStart() {
  UsbDeviceResource* const resource =
      GetUsbDeviceResource(parameters_->handle.handle);
  if (!resource) {
    CompleteWithError(kErrorNoDevice);
    return;
  }

  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(&UsbResetDeviceFunction::OnStartResest, this, resource));
}

void UsbResetDeviceFunction::OnStartResest(UsbDeviceResource* resource) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  resource->ResetDevice(
      base::Bind(&UsbResetDeviceFunction::OnCompletedFileThread, this));
}

void UsbResetDeviceFunction::OnCompletedFileThread(bool success) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&UsbResetDeviceFunction::OnCompleted, this, success));
  return;
}

void UsbResetDeviceFunction::OnCompleted(bool success) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (!success) {
    UsbDeviceResource* const resource =
        GetUsbDeviceResource(parameters_->handle.handle);
    if (!resource) {
      CompleteWithError(kErrorNoDevice);
      return;
    }
    // Close the device now because the handle is invalid after an
    // unsuccessful reset.
    resource->Close(base::Bind(&UsbResetDeviceFunction::OnError, this));
    RemoveUsbDeviceResource(parameters_->handle.handle);
    return;
  }
  SetResult(Value::CreateBooleanValue(true));
  AsyncWorkCompleted();
}

void UsbResetDeviceFunction::OnError() {
  SetError(kErrorResetDevice);
  SetResult(Value::CreateBooleanValue(false));
  AsyncWorkCompleted();
}

}  // namespace extensions
