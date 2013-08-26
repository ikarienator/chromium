// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/hid/hid_api.h"

#include <string>
#include <vector>

#include "chrome/browser/extensions/api/api_resource_manager.h"
#include "chrome/browser/extensions/api/hid/hid_device_resource.h"
#include "chrome/common/extensions/api/hid.h"
#include "chrome/common/extensions/permissions/usb_device_permission.h"
#include "device/hid/hid_connection.h"
#include "device/hid/hid_device.h"
#include "device/hid/hid_service.h"
#include "extensions/common/permissions/permissions_data.h"
#include "net/base/io_buffer.h"

namespace hid = extensions::api::hid;

using device::HidConnection;
using device::HidDevice;
using device::HidService;
using device::PlatformHidDeviceInfo;

namespace {

const char kErrorPermissionDenied[] = "Permission to access device was denied.";
const char kErrorDeviceNotFound[] = "HID device not found.";
const char kErrorFailedToOpenDevice[] = "Failed to open HID device.";
const char kErrorConnectionNotFound[] = "Connection not established.";
const char kErrorTransfer[] = "Transfer failed.";

base::Value* PopulateHidDevice(scoped_refptr<HidDevice> info) {
  hid::HidDeviceInfo device_info;
  device_info.device_id = info->device_id();
  device_info.product_id = info->product_id();
  device_info.vendor_id = info->vendor_id();
  return device_info.ToValue().release();
}

base::Value* PopulateHidConnection(int connection_id,
                                   scoped_refptr<HidConnection> connection) {
  hid::HidConnection connection_value;
  connection_value.connection_id = connection_id;
  return connection_value.ToValue().release();
}

}  // namespace

namespace extensions {

HidAsyncApiFunction::HidAsyncApiFunction() : manager_(NULL) {}

HidAsyncApiFunction::~HidAsyncApiFunction() {}

bool HidAsyncApiFunction::PrePrepare() {
  manager_ = ApiResourceManager<HidDeviceResource>::Get(GetProfile());
  if (!manager_) return false;
  set_work_thread_id(content::BrowserThread::FILE);
  return true;
}

bool HidAsyncApiFunction::Respond() { return error_.empty(); }

HidDeviceResource* HidAsyncApiFunction::GetHidDeviceResource(
    int api_resource_id) {
  return manager_->Get(extension_->id(), api_resource_id);
}

void HidAsyncApiFunction::RemoveHidDeviceResource(int api_resource_id) {
  manager_->Remove(extension_->id(), api_resource_id);
}

void HidAsyncApiFunction::CompleteWithError(const std::string& error) {
  SetError(error);
  AsyncWorkCompleted();
}

HidGetDevicesFunction::HidGetDevicesFunction() {}

HidGetDevicesFunction::~HidGetDevicesFunction() {}

bool HidGetDevicesFunction::Prepare() {
  parameters_ = hid::GetDevices::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(parameters_.get());
  return true;
}

void HidGetDevicesFunction::AsyncWorkStart() {
  const uint16_t vendor_id = parameters_->options.vendor_id;
  const uint16_t product_id = parameters_->options.product_id;
  UsbDevicePermission::CheckParam param(
      vendor_id, product_id, UsbDevicePermissionData::UNSPECIFIED_INTERFACE);
  if (!PermissionsData::CheckAPIPermissionWithParam(
          GetExtension(), APIPermission::kUsbDevice, &param)) {
    LOG(WARNING) << "Insufficient permissions to access device.";
    CompleteWithError(kErrorPermissionDenied);
    return;
  }

  HidService* service = HidService::GetInstance();
  std::vector<scoped_refptr<HidDevice> > devices;
  service->UpdateDevices();
  service->GetDevices(&devices);

  scoped_ptr<base::ListValue> result(new base::ListValue());
  for (std::vector<scoped_refptr<HidDevice> >::iterator it = devices.begin();
       it != devices.end(); it++) {
    if ((*it)->product_id() == product_id &&
        (*it)->vendor_id() == vendor_id)
      result->Append(PopulateHidDevice(*it));
  }
  SetResult(result.release());
  AsyncWorkCompleted();
}

HidConnectFunction::HidConnectFunction() {}

HidConnectFunction::~HidConnectFunction() {}

bool HidConnectFunction::Prepare() {
  parameters_ = hid::Connect::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(parameters_.get());
  return true;
}

void HidConnectFunction::AsyncWorkStart() {
  HidService* service = HidService::GetInstance();
  scoped_refptr<HidDevice> device =
      service->GetDevice(parameters_->device_info.device_id);
  if (!device) {
    CompleteWithError(kErrorDeviceNotFound);
    return;
  }
  if (device->vendor_id() != parameters_->device_info.vendor_id ||
      device->product_id() != parameters_->device_info.product_id) {
    CompleteWithError(kErrorDeviceNotFound);
    return;
  }
  scoped_refptr<HidConnection> connection = device->Connect();
  if (!connection) {
    CompleteWithError(kErrorFailedToOpenDevice);
    return;
  }
  int connection_id =
      manager_->Add(new HidDeviceResource(extension_->id(), connection));
  SetResult(PopulateHidConnection(connection_id, connection));
  AsyncWorkCompleted();
}

HidDisconnectFunction::HidDisconnectFunction() {}

HidDisconnectFunction::~HidDisconnectFunction() {}

bool HidDisconnectFunction::Prepare() {
  parameters_ = hid::Disconnect::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(parameters_.get());
  return true;
}

void HidDisconnectFunction::AsyncWorkStart() {
  int connection_id = parameters_->connection.connection_id;
  HidDeviceResource* resource =
      manager_->Get(extension_->id(), connection_id);
  if (!resource) {
    CompleteWithError(kErrorConnectionNotFound);
    return;
  }
  manager_->Remove(extension_->id(), connection_id);
  AsyncWorkCompleted();
}

HidReadFunction::HidReadFunction() {}

HidReadFunction::~HidReadFunction() {}

bool HidReadFunction::Prepare() {
  parameters_ = hid::Read::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(parameters_.get());
  return true;
}

void HidReadFunction::AsyncWorkStart() {
  int connection_id = parameters_->connection.connection_id;
  HidDeviceResource* resource =
      manager_->Get(extension_->id(), connection_id);
  if (!resource) {
    CompleteWithError(kErrorConnectionNotFound);
    return;
  }

  buffer_ = new net::IOBuffer(parameters_->size);
  resource->connection()->Read(buffer_,
                               parameters_->size,
                               base::Bind(&HidReadFunction::OnFinished, this));
}

void HidReadFunction::OnFinished(bool success, size_t bytes) {
  if (!success) {
    CompleteWithError(kErrorTransfer);
    return;
  }

  SetResult(base::BinaryValue::CreateWithCopiedBuffer(buffer_->data(), bytes));
  AsyncWorkCompleted();
}

HidWriteFunction::HidWriteFunction() {}

HidWriteFunction::~HidWriteFunction() {}

bool HidWriteFunction::Prepare() {
  parameters_ = hid::Write::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(parameters_.get());
  return true;
}

void HidWriteFunction::AsyncWorkStart() {
  int connection_id = parameters_->connection.connection_id;
  HidDeviceResource* resource =
      manager_->Get(extension_->id(), connection_id);
  if (!resource) {
    CompleteWithError(kErrorConnectionNotFound);
    return;
  }

  scoped_refptr<net::IOBuffer> buffer(
      new net::WrappedIOBuffer(parameters_->data.c_str()));
  memcpy(buffer->data(),
         parameters_->data.c_str(),
         parameters_->data.size());
  resource->connection()->Write(
      buffer,
      parameters_->data.size(),
      base::Bind(&HidWriteFunction::OnFinished, this));
}

void HidWriteFunction::OnFinished(bool success, size_t bytes) {
  if (!success) {
    CompleteWithError(kErrorTransfer);
    return;
  }
  AsyncWorkCompleted();
}

HidGetFeatureReportFunction::HidGetFeatureReportFunction() {}

HidGetFeatureReportFunction::~HidGetFeatureReportFunction() {}

bool HidGetFeatureReportFunction::Prepare() {
  parameters_ = hid::GetFeatureReport::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(parameters_.get());
  return true;
}

void HidGetFeatureReportFunction::AsyncWorkStart() {
  int connection_id = parameters_->connection.connection_id;
  HidDeviceResource* resource =
      manager_->Get(extension_->id(), connection_id);
  if (!resource) {
    CompleteWithError(kErrorConnectionNotFound);
    return;
  }
  buffer_ = new net::IOBuffer(parameters_->size);
  resource->connection()->GetFeatureReport(
      buffer_,
      parameters_->size,
      base::Bind(&HidGetFeatureReportFunction::OnFinished, this));
}

void HidGetFeatureReportFunction::OnFinished(bool success, size_t bytes) {
  if (!success) {
    CompleteWithError(kErrorTransfer);
    return;
  }

  SetResult(base::BinaryValue::CreateWithCopiedBuffer(buffer_->data(), bytes));
  AsyncWorkCompleted();
}

HidSendFeatureReportFunction::HidSendFeatureReportFunction() {}

HidSendFeatureReportFunction::~HidSendFeatureReportFunction() {}

bool HidSendFeatureReportFunction::Prepare() {
  parameters_ = hid::SendFeatureReport::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(parameters_.get());
  return true;
}

void HidSendFeatureReportFunction::AsyncWorkStart() {
  int connection_id = parameters_->connection.connection_id;
  HidDeviceResource* resource = manager_->Get(extension_->id(), connection_id);
  if (!resource) {
    CompleteWithError(kErrorConnectionNotFound);
    return;
  }
  scoped_refptr<net::IOBuffer> buffer(
      new net::WrappedIOBuffer(parameters_->data.c_str()));
  resource->connection()->SendFeatureReport(
      buffer,
      parameters_->data.size(),
      base::Bind(&HidSendFeatureReportFunction::OnFinished, this));
}

void HidSendFeatureReportFunction::OnFinished(bool success, size_t bytes) {
  if (!success) {
    CompleteWithError(kErrorTransfer);
    return;
  }
  AsyncWorkCompleted();
}

}  // namespace extensions
