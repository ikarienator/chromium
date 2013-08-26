// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_HID_HID_SERVICE_H_
#define CHROME_BROWSER_HID_HID_SERVICE_H_

#include <map>
#include <string>
#include <vector>

#include "build/build_config.h"
#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/memory/singleton.h"
#include "base/strings/string16.h"

namespace base {

template <class T>
class DeleteHelper;

}  // namespace base

template <typename T>
struct DefaultSingletonTraits;

class HidContext;
class HidDevice;

struct HidInfo {
  uint device_id;
  std::string path;
  uint16 vendor_id;
  uint16 product_id;
  uint16 release_number;
  uint16 usage_page;
  uint16 usage;
  int interface_number;

  HidInfo();
  virtual ~HidInfo();
};

class HidService {
 public:
  HidService();
  virtual ~HidService();

  // Must be called on FILE thread.
  static HidService* GetInstance();

  virtual ScopedVector<HidInfo> EnumerateHidOverUsbDevices(
      uint16 vendor_id, uint16 product_id, int interface_id);

  // virtual scoped_refptr<HidDevice> OpenDevice(const HidInfo& info);

 private:
  friend struct DefaultSingletonTraits<HidService>;
  friend class base::DeleteHelper<HidService>;

  scoped_ptr<HidContext> context_;

  int next_unique_id_;
  std::map<std::string, int> devices_;

  DISALLOW_COPY_AND_ASSIGN(HidService);
};

#endif  // CHROME_BROWSER_HID_HID_SERVICE_H_
