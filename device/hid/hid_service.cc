// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/hid/hid_service.h"

#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/logging.h"
#include "base/memory/scoped_vector.h"
#include "chrome/browser/chrome_notification_types.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_service.h"
#include "third_party/hidapi/src/hidapi/hidapi.h"

using content::BrowserThread;

namespace {

class ExitObserver : public content::NotificationObserver {
 public:
  explicit ExitObserver(HidService* service) : service_(service) {
    BrowserThread::PostTask(
            BrowserThread::UI, FROM_HERE,
            base::Bind(&content::NotificationRegistrar::Add,
                       base::Unretained(&registrar_), this,
                       chrome::NOTIFICATION_APP_TERMINATING,
                       content::NotificationService::AllSources()));
  }

 private:
  // content::NotificationObserver
  virtual void Observe(
      int type, const class content::NotificationSource& source,
      const class content::NotificationDetails& details) OVERRIDE {
    if (type == chrome::NOTIFICATION_APP_TERMINATING) {
      BrowserThread::DeleteSoon(BrowserThread::FILE, FROM_HERE, service_);
      delete this;
    }
  }
  HidService* service_;
  content::NotificationRegistrar registrar_;
};

}  // namespace

class HidContext {};

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

HidService::~HidService() {
  LOG(ERROR) << "~HidService";
}

HidService* HidService::GetInstance() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  // HidService deletes itself upon APP_TERMINATING.
  HidService* service =
      Singleton<HidService, LeakySingletonTraits<HidService> >::get();
  new ExitObserver(service);
  return service;
}

ScopedVector<HidInfo> HidService::EnumerateHidOverUsbDevices(uint16 vendor_id,
                                                             uint16 product_id,
                                                             int interface_id) {
  hid_device_info* info = hid_enumerate(0, 0);
  std::set<std::string> paths, to_remove;
  ScopedVector<HidInfo> result;
  for (hid_device_info* i = info; i != NULL; i = i->next) {
    std::string path = i->path;
    if (path.size() == 0) continue;

    scoped_ptr<HidInfo> info(new HidInfo);

    info->interface_number = i->interface_number;
    info->product_id = i->product_id;
    info->vendor_id = i->vendor_id;
    info->release_number = i->release_number;
    info->usage = i->usage;
    info->usage_page = i->usage_page;

    if (!ContainsKey(devices_, path)) {
      devices_[path] = next_unique_id_++;
    }

    paths.insert(path);
    info->device_id = devices_[path];
    LOG(ERROR) << "path = " << path;
    result.push_back(info.release());
  }
  for (std::map<std::string, int>::iterator it = devices_.begin();
       it != devices_.end(); ++it) {
    if (!ContainsKey(paths, it->first)) {
      to_remove.insert(it->first);
    }
  }
  for (std::set<std::string>::iterator it = to_remove.begin();
       it != to_remove.end(); ++it) {
    devices_.erase(*it);
  }
  hid_free_enumeration(info);
  return result.Pass();
}
