// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/hid/hid_api.h"

#include <string>

#include "chrome/browser/extensions/api/api_resource_manager.h"
#include "chrome/browser/extensions/api/hid/hid_device_resource.h"
#include "chrome/common/extensions/api/hid.h"
#include "chrome/common/extensions/permissions/permissions_data.h"
#include "chrome/common/extensions/permissions/usb_device_permission.h"
#include "device/hid/hid_service.h"
#include "net/base/io_buffer.h"

namespace hid = extensions::api::hid;

namespace {

const char kErrorPermissionDenied[] = "Permission to access device was denied";

base::Value* PopulateHidDevice(HidInfo* info) {
  hid::HidDeviceInfo device_info;
  device_info.device_id = info->device_id;
  device_info.interface_id = info->interface_number;
  device_info.product_id = info->product_id;
  device_info.vendor_id = info->vendor_id;
  return device_info.ToValue().release();
}

}  // namespace

namespace extensions {

HidAsyncApiFunction::HidAsyncApiFunction() : manager_(NULL) {}

HidAsyncApiFunction::~HidAsyncApiFunction() {}

bool HidAsyncApiFunction::PrePrepare() {
  manager_ = ApiResourceManager<HidDeviceResource>::Get(profile());
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
  int interface_id = parameters_->options.interface_id.get()
                         ? *parameters_->options.interface_id.get()
                         : UsbDevicePermissionData::ANY_INTERFACE;
  UsbDevicePermission::CheckParam param(vendor_id, product_id, interface_id);
//  if (!PermissionsData::CheckAPIPermissionWithParam(
//           GetExtension(), APIPermission::kUsbDevice, &param)) {
//    LOG(WARNING) << "Insufficient permissions to access device.";
//    CompleteWithError(kErrorPermissionDenied);
//    return;
//  }

  HidService* service = HidService::GetInstance();
  ScopedVector<HidInfo> info =
      service->EnumerateHidOverUsbDevices(vendor_id, product_id, interface_id);

  scoped_ptr<base::ListValue> result(new base::ListValue());
  for (ScopedVector<HidInfo>::iterator it = info.begin(); it != info.end();
       it++) {
    result->Append(PopulateHidDevice(*it));
  }
  SetResult(result.release());
  AsyncWorkCompleted();
}

HidOpenDeviceFunction::HidOpenDeviceFunction() {}

HidOpenDeviceFunction::~HidOpenDeviceFunction() {}

bool HidOpenDeviceFunction::Prepare() {
  parameters_ = hid::OpenDevice::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(parameters_.get());
  return true;
}

void HidOpenDeviceFunction::AsyncWorkStart() { AsyncWorkCompleted(); }

HidCloseDeviceFunction::HidCloseDeviceFunction() {}

HidCloseDeviceFunction::~HidCloseDeviceFunction() {}

bool HidCloseDeviceFunction::Prepare() {
  parameters_ = hid::CloseDevice::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(parameters_.get());
  return true;
}

void HidCloseDeviceFunction::AsyncWorkStart() { AsyncWorkCompleted(); }

HidReadFunction::HidReadFunction() {}

HidReadFunction::~HidReadFunction() {}

bool HidReadFunction::Prepare() {
  parameters_ = hid::Read::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(parameters_.get());
  return true;
}

void HidReadFunction::AsyncWorkStart() { AsyncWorkCompleted(); }

HidWriteFunction::HidWriteFunction() {}

HidWriteFunction::~HidWriteFunction() {}

bool HidWriteFunction::Prepare() {
  parameters_ = hid::Write::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(parameters_.get());
  return true;
}

void HidWriteFunction::AsyncWorkStart() { AsyncWorkCompleted(); }

HidGetFeatureReportFunction::HidGetFeatureReportFunction() {}

HidGetFeatureReportFunction::~HidGetFeatureReportFunction() {}

bool HidGetFeatureReportFunction::Prepare() {
  parameters_ = hid::GetFeatureReport::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(parameters_.get());
  return true;
}

void HidGetFeatureReportFunction::AsyncWorkStart() { AsyncWorkCompleted(); }

HidSendFeatureReportFunction::HidSendFeatureReportFunction() {}

HidSendFeatureReportFunction::~HidSendFeatureReportFunction() {}

bool HidSendFeatureReportFunction::Prepare() {
  parameters_ = hid::SendFeatureReport::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(parameters_.get());
  return true;
}

void HidSendFeatureReportFunction::AsyncWorkStart() { AsyncWorkCompleted(); }

}  // namespace extensions
