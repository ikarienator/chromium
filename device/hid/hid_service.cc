// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/hid/hid_service.h"

#include <vector>

#include "base/logging.h"
#include "base/memory/scoped_vector.h"
#include "base/memory/singleton.h"
#include "base/threading/thread_restrictions.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace device {

HidDeviceWatcher::HidDeviceWatcher(HidService* service) : service_(service) {}
HidDeviceWatcher::~HidDeviceWatcher() {}

void HidDeviceWatcher::DeviceAdd(HidDeviceInfo device) const {
  service_->DeviceAdd(device);
}
void HidDeviceWatcher::DeviceRemove(std::string device_id) const {
  service_->DeviceRemove(device_id);
}

HidDeviceInfo::HidDeviceInfo()
    : vendor_id(0),
      product_id(0),
      usage_page(0),
      usage(0) {}

HidDeviceInfo::~HidDeviceInfo() {}

HidService::HidService() {
  base::ThreadRestrictions::AssertIOAllowed();
  DCHECK(thread_checker_.CalledOnValidThread());
  InitializeDeviceWatcher();
  base::MessageLoop::current()->AddDestructionObserver(this);
}

HidService::HidService(scoped_refptr<HidDeviceWatcher> watcher) {
  base::ThreadRestrictions::AssertIOAllowed();
  DCHECK(thread_checker_.CalledOnValidThread());
  watcher_ = watcher;
}

HidService::~HidService() {
  DCHECK(thread_checker_.CalledOnValidThread());
  base::MessageLoop::current()->RemoveDestructionObserver(this);
}

HidService* HidService::GetInstance() {
  base::ThreadRestrictions::AssertIOAllowed();
  return Singleton<HidService, DefaultSingletonTraits<HidService> >::get();
}

void HidService::GetDevices(std::vector<HidDeviceInfo>* devices) {
  DCHECK(thread_checker_.CalledOnValidThread());
  STLClearObject(devices);
  for (DeviceMap::iterator it = devices_.begin();
      it != devices_.end();
      ++it) {
    devices->push_back(it->second);
  }
}

void HidService::DeviceAdd(HidDeviceInfo info) {
  DCHECK(thread_checker_.CalledOnValidThread());
  devices_[info.device_id] = info;
}

void HidService::DeviceRemove(std::string device_id) {
  DCHECK(thread_checker_.CalledOnValidThread());
  devices_.erase(device_id);
}

void HidService::WillDestroyCurrentMessageLoop() {
  DCHECK(thread_checker_.CalledOnValidThread());
  watcher_ = NULL;
  devices_.clear();
}

}  // namespace device
