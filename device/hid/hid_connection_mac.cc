// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/hid/hid_connection_mac.h"

#include "device/hid/hid_utils_mac.h"
#include "net/base/io_buffer.h"

namespace device {

HidConnectionMac::HidConnectionMac(IOHIDDeviceRef device)
    : device_(device) {
  IOHIDDeviceOpen(device_.get(), kIOHIDOptionsTypeNone);
}

HidConnectionMac::~HidConnectionMac() {
  IOHIDDeviceClose(device_.get(), kIOHIDOptionsTypeNone);
}

void HidConnectionMac::Read(const HidReadCallback& callback) {
}
void HidConnectionMac::Write(uint8 report_id,
                             scoped_refptr<net::IOBuffer> buffer,
                             size_t size,
                             const HidWriteCallback& callback) {
//  IOHIDDeviceSetReportWithCallback(
//      device_.get(),
//      kIOHIDReportTypeInput,
//      report_id,
//      buffer->data(),
//      size,
//
//    );
}
void HidConnectionMac::GetFeatureReport(const HidReadCallback& callback) {
}
void HidConnectionMac::SendFeatureReport(uint8 report_id,
                                         scoped_refptr<net::IOBuffer> buffer,
                                         size_t size,
                                         const HidWriteCallback& callback) {
}

}  // namespace device
