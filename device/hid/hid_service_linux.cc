// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/hid/hid_service.h"

#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/logging.h"
#include "base/memory/scoped_vector.h"
#include "content/public/browser/browser_thread.h"


using content::BrowserThread;

namespace content {

class NotificationDetails;
class NotificationSource;

}  // namespace content

HidInfo::HidInfo()
    : device_id(0),
      vendor_id(0),
      product_id(0),
      release_number(0),
      usage_page(0),
      usage(0),
      interface_number(0) {}

HidInfo::~HidInfo() {}

HidService::HidService() : next_unique_id_(0) {}

HidService::~HidService() {}

HidService* HidService::GetInstance() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  // HidService deletes itself upon APP_TERMINATING.
  return Singleton<HidService, LeakySingletonTraits<HidService> >::get();
}
