// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/hid/hid_service.h"

#include <map>
#include <set>
#include <string>

#include "base/logging.h"
#include "base/memory/scoped_vector.h"
#include "base/memory/singleton.h"
#include "base/threading/thread_checker.h"
#include "base/threading/thread_restrictions.h"
#include "device/hid/hid_device.h"
#include "third_party/hidapi/hidapi.h"

namespace device {

namespace {

struct HidEnumerationDeleter {
  inline void operator()(PlatformHidDeviceInfo info) const {
    hid_free_enumeration(info);
  }
};

}  // namespace

HidService::HidService() : next_unique_id_(0) {
  base::ThreadRestrictions::AssertIOAllowed();
  DCHECK(thread_checker_.CalledOnValidThread());
}
HidService::~HidService() {
  // No thread unsafe resource to cleanup. Does not need the thread checker.
}

HidService* HidService::GetInstance() {
  return Singleton<HidService>::get();
}

void HidService::UpdateDevices() {
  base::ThreadRestrictions::AssertIOAllowed();
  DCHECK(thread_checker_.CalledOnValidThread());
  scoped_ptr<hid_device_info, HidEnumerationDeleter> info(hid_enumerate(0, 0));
  std::map<std::string, hid_device_info*> new_list;
  std::vector<int> to_remove;
  for (PlatformHidDeviceInfo i = info.get(); i; i = i->next) {
    new_list[i->path] = i;
  }

  for (DeviceMap::iterator it = devices_.begin(); it != devices_.end(); ++it) {
    if (!ContainsKey(new_list, it->second->path())) {
      to_remove.push_back(it->first);
    } else {
      new_list.erase(it->second->path());
    }
  }

  for (std::vector<int>::iterator it = to_remove.begin(); it != to_remove.end();
       ++it) {
    devices_.erase(*it);
  }

  for (std::map<std::string, PlatformHidDeviceInfo>::iterator it =
           new_list.begin();
       it != new_list.end(); ++it) {
    devices_[next_unique_id_] =
        make_scoped_refptr(new HidDevice(next_unique_id_, it->second));
    ++next_unique_id_;
  }
}

void HidService::GetDevices(
    std::vector<scoped_refptr<HidDevice> >* devices) const {
  DCHECK(thread_checker_.CalledOnValidThread());
  STLClearObject(devices);
  for (DeviceMap::const_iterator it = devices_.begin(); it != devices_.end();
       ++it) {
    devices->push_back(it->second);
  }
}

scoped_refptr<HidDevice> HidService::GetDevice(int device_id) const {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!ContainsKey(devices_, device_id)) {
    return scoped_refptr<HidDevice>();
  }
  return devices_.find(device_id)->second;
}

}  // namespace device
