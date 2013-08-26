// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/hid/hid_device_resource.h"

#include <string>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/memory/ref_counted.h"
#include "device/hid/hid_device.h"

namespace extensions {

static base::LazyInstance<ProfileKeyedAPIFactory<
        ApiResourceManager<HidDeviceResource> > >
            g_factory = LAZY_INSTANCE_INITIALIZER;

// static
template <>
ProfileKeyedAPIFactory<ApiResourceManager<HidDeviceResource> >*
ApiResourceManager<HidDeviceResource>::GetFactoryInstance() {
  return &g_factory.Get();
}

HidDeviceResource::HidDeviceResource(const std::string& owner_extension_id,
                                     scoped_refptr<HidDevice> device)
    : ApiResource(owner_extension_id), device_(device) {}

HidDeviceResource::~HidDeviceResource() {}

}  // namespace extensions
