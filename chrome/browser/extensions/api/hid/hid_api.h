// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_HID_HID_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_HID_HID_API_H_

#include <string>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "chrome/browser/extensions/api/api_function.h"
#include "chrome/browser/extensions/api/api_resource_manager.h"
#include "chrome/browser/extensions/api/hid/hid_device_resource.h"
#include "chrome/browser/extensions/extension_function_histogram_value.h"
#include "chrome/common/extensions/api/hid.h"

namespace net {

class IOBuffer;

}  // namespace net

namespace extensions {

class HidAsyncApiFunction : public AsyncApiFunction {
 public:
  HidAsyncApiFunction();

  virtual bool PrePrepare() OVERRIDE;
  virtual bool Respond() OVERRIDE;

 protected:
  virtual ~HidAsyncApiFunction();

  HidConnectionResource* GetHidConnectionResource(int api_resource_id);
  void RemoveHidConnectionResource(int api_resource_id);

  void CompleteWithError(const std::string& error);

  ApiResourceManager<HidConnectionResource>* manager_;

 private:
  DISALLOW_COPY_AND_ASSIGN(HidAsyncApiFunction);
};

class HidGetDevicesFunction : public HidAsyncApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("hid.getDevices", HID_GETDEVICES);

  HidGetDevicesFunction();

 protected:
  virtual bool Prepare() OVERRIDE;
  virtual void AsyncWorkStart() OVERRIDE;

  virtual ~HidGetDevicesFunction();

  scoped_ptr<base::ListValue> result_;
  scoped_ptr<extensions::api::hid::GetDevices::Params> parameters_;

 private:
  DISALLOW_COPY_AND_ASSIGN(HidGetDevicesFunction);
};

class HidConnectFunction : public HidAsyncApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("hid.connect", HID_CONNECT);

  HidConnectFunction();

 protected:
  virtual bool Prepare() OVERRIDE;
  virtual void AsyncWorkStart() OVERRIDE;

 private:
  virtual ~HidConnectFunction();

  scoped_ptr<base::ListValue> result_;
  scoped_ptr<extensions::api::hid::Connect::Params> parameters_;

  DISALLOW_COPY_AND_ASSIGN(HidConnectFunction);
};

class HidDisconnectFunction : public HidAsyncApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("hid.disconnect", HID_DISCONNECT);

  HidDisconnectFunction();

 protected:
  virtual bool Prepare() OVERRIDE;
  virtual void AsyncWorkStart() OVERRIDE;

 private:
  virtual ~HidDisconnectFunction();

  scoped_ptr<base::ListValue> result_;
  scoped_ptr<extensions::api::hid::Disconnect::Params> parameters_;

  DISALLOW_COPY_AND_ASSIGN(HidDisconnectFunction);
};

class HidReadFunction : public HidAsyncApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("hid.read", HID_READ);

  HidReadFunction();

 protected:
  virtual bool Prepare() OVERRIDE;
  virtual void AsyncWorkStart() OVERRIDE;

 private:
  virtual ~HidReadFunction();

  void OnFinished(bool success, size_t bytes);

  scoped_refptr<net::IOBuffer> buffer_;
  scoped_ptr<base::ListValue> result_;
  scoped_ptr<extensions::api::hid::Read::Params> parameters_;

  DISALLOW_COPY_AND_ASSIGN(HidReadFunction);
};

class HidWriteFunction : public HidAsyncApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("hid.write", HID_WRITE);

  HidWriteFunction();

 protected:
  virtual bool Prepare() OVERRIDE;
  virtual void AsyncWorkStart() OVERRIDE;

 private:
  virtual ~HidWriteFunction();

  void OnFinished(bool success, size_t bytes);

  scoped_ptr<base::ListValue> result_;
  scoped_ptr<extensions::api::hid::Write::Params> parameters_;

  DISALLOW_COPY_AND_ASSIGN(HidWriteFunction);
};

class HidGetFeatureReportFunction : public HidAsyncApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("hid.getFeatureReport", HID_GETFEATUREREPORT);

  HidGetFeatureReportFunction();

 protected:
  virtual bool Prepare() OVERRIDE;
  virtual void AsyncWorkStart() OVERRIDE;

 private:
  virtual ~HidGetFeatureReportFunction();

  void OnFinished(bool success, size_t bytes);

  scoped_refptr<net::IOBuffer> buffer_;
  scoped_ptr<base::ListValue> result_;
  scoped_ptr<extensions::api::hid::GetFeatureReport::Params> parameters_;

  DISALLOW_COPY_AND_ASSIGN(HidGetFeatureReportFunction);
};

class HidSendFeatureReportFunction : public HidAsyncApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("hid.sendFeatureReport", HID_SENDFEATUREREPORT);

  HidSendFeatureReportFunction();

 protected:
  virtual bool Prepare() OVERRIDE;
  virtual void AsyncWorkStart() OVERRIDE;

 private:
  virtual ~HidSendFeatureReportFunction();

  void OnFinished(bool success, size_t bytes);

  scoped_ptr<base::ListValue> result_;
  scoped_ptr<extensions::api::hid::SendFeatureReport::Params> parameters_;

  DISALLOW_COPY_AND_ASSIGN(HidSendFeatureReportFunction);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_HID_HID_API_H_
