// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/hid/hid_connection.h"

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/location.h"
#include "base/memory/ref_counted.h"
#include "base/threading/thread_restrictions.h"
#include "base/threading/worker_pool.h"
#include "device/hid/hid_connection.h"
#include "net/base/io_buffer.h"
#include "third_party/hidapi/hidapi.h"

namespace device {

namespace {

typedef int HID_API_EXPORT (*HidReadFunc)(PlatformHidDevice dev,
                                          uint8* data,
                                          size_t length);

typedef int HID_API_EXPORT (*HidWriteFunc)(PlatformHidDevice dev,
                                           const uint8* data,
                                           size_t length);

template <HidReadFunc func>
void HidRead(PlatformHidDevice platform_device,
           scoped_refptr<net::IOBuffer> buffer, size_t size,
           int* actual_bytes) {
  *actual_bytes = func(platform_device,
                       reinterpret_cast<uint8*>(buffer->data()),
                       size);
}

template <HidWriteFunc func>
void HidWrite(PlatformHidDevice platform_device,
           scoped_refptr<net::IOBuffer> buffer, size_t size,
           int* actual_bytes) {
  *actual_bytes = func(platform_device,
                       reinterpret_cast<const uint8*>(buffer->data()),
                       size);
}

void OnHidIOFinished(int* actual_bytes, HidIOCallback callback) {
  callback.Run(*actual_bytes >= 0, *actual_bytes);
}

}  // namespace

HidConnection::HidConnection(PlatformHidDevice platform_device)
    : platform_device_(platform_device) {
  base::ThreadRestrictions::AssertIOAllowed();
  DCHECK(thread_checker_.CalledOnValidThread());
}

HidConnection::~HidConnection() {
  DCHECK(thread_checker_.CalledOnValidThread());
  hid_close(platform_device_);
}

void HidConnection::Read(scoped_refptr<net::IOBuffer> buffer, size_t size,
                         const HidIOCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  int* actual_bytes = new int(0);

  base::WorkerPool::PostTaskAndReply(
      FROM_HERE,
      base::Bind(HidRead<hid_read>,
                 platform_device_,
                 buffer,
                 size,
                 base::Unretained(actual_bytes)),
      base::Bind(OnHidIOFinished, base::Owned(actual_bytes), callback),
      true);
}

void HidConnection::Write(scoped_refptr<net::IOBuffer> buffer, size_t size,
                          const HidIOCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  int* actual_bytes = new int(0);

  base::WorkerPool::PostTaskAndReply(
        FROM_HERE,
        base::Bind(HidWrite<hid_write>,
                   platform_device_,
                   buffer,
                   size,
                   base::Unretained(actual_bytes)),
        base::Bind(OnHidIOFinished, base::Owned(actual_bytes), callback),
        true);
}

void HidConnection::GetFeatureReport(scoped_refptr<net::IOBuffer> buffer,
                                     size_t size,
                                     const HidIOCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  int* actual_bytes = new int(0);

  base::WorkerPool::PostTaskAndReply(
        FROM_HERE,
        base::Bind(HidRead<hid_get_feature_report>,
                   platform_device_,
                   buffer,
                   size,
                   base::Unretained(actual_bytes)),
        base::Bind(OnHidIOFinished, base::Owned(actual_bytes), callback),
        true);
}

void HidConnection::SendFeatureReport(scoped_refptr<net::IOBuffer> buffer,
                                      size_t size,
                                      const HidIOCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  int* actual_bytes = new int(0);

  base::WorkerPool::PostTaskAndReply(
          FROM_HERE,
          base::Bind(HidWrite<hid_send_feature_report>,
                     platform_device_,
                     buffer,
                     size,
                     base::Unretained(actual_bytes)),
          base::Bind(OnHidIOFinished, base::Owned(actual_bytes), callback),
          true);
}

}  // namespace device
