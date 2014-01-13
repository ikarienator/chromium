// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_HID_HID_EVENT_DISPATCHER_H_
#define CHROME_BROWSER_EXTENSIONS_API_HID_HID_EVENT_DISPATCHER_H_

#include "chrome/browser/extensions/api/api_resource_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/api/hid.h"

namespace extensions {
namespace api {

class HidEventDispatcher : public ProfileKeyedAPI {
 public:
  explicit HidEventDispatcher(Profile* profile);
  virtual ~HidEventDispatcher();

  // ProfileKeyedAPI implementation.
  static ProfileKeyedAPIFactory<SerialEventDispatcher>* GetFactoryInstance();

 private:
  DISALLOW_COPY_AND_ASSIGN(HidEventDispatcher);
};

}  // namespace api
}  // namespace extensions
#endif  // CHROME_BROWSER_EXTENSIONS_API_HID_HID_EVENT_DISPATCHER_H_
