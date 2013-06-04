// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/usb/usb_service.h"

#include <set>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "chrome/browser/usb/usb_device.h"
#include "content/public/browser/browser_thread.h"
#include "third_party/libusb/src/libusb/libusb.h"

#if defined(OS_CHROMEOS)
#include "base/chromeos/chromeos_version.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/permission_broker_client.h"
#endif  // defined(OS_CHROMEOS)

using std::vector;
using std::set;
using content::BrowserThread;

// The UsbEventHandler works around a design flaw in the libusb interface. There
// is currently no way to signal to libusb that any caller into one of the event
// handler calls should return without handling any events.
class UsbEventHandler : public base::PlatformThread::Delegate {
 public:
  explicit UsbEventHandler(PlatformUsbContext context)
      : running_(true), context_(context) {
    base::PlatformThread::CreateNonJoinable(0, this);
  }

  virtual ~UsbEventHandler() {}

  virtual void ThreadMain() OVERRIDE {
    base::PlatformThread::SetName("UsbEventHandler");

    DLOG(INFO) << "UsbEventHandler started.";
    while (running_) {
      libusb_handle_events(context_);
    }
    DLOG(INFO) << "UsbEventHandler shutting down.";
    libusb_exit(context_);

    delete this;
  }

  void Stop() {
    running_ = false;
  }

 private:
  bool running_;
  PlatformUsbContext context_;

  DISALLOW_EVIL_CONSTRUCTORS(UsbEventHandler);
};

// RefCountedPlatformUsbDevice takes care of managing the underlying reference
// count of a single PlatformUsbDevice. This allows us to construct things
// like vectors of RefCountedPlatformUsbDevices and not worry about having to
// explicitly unreference them after use.
class RefCountedPlatformUsbDevice
    : public base::RefCounted<RefCountedPlatformUsbDevice> {
 public:
  explicit RefCountedPlatformUsbDevice(PlatformUsbDevice device,
                                       int unique_id);
  PlatformUsbDevice device() const { return device_; }
  int unique_id() const { return unique_id_; }

  scoped_refptr<UsbDevice> OpenDevice(UsbService* service);
  void CloseDevice(UsbDevice* device);

 private:
  virtual ~RefCountedPlatformUsbDevice();
  friend class base::RefCounted<RefCountedPlatformUsbDevice>;
  std::vector<scoped_refptr<UsbDevice> > handles_;
  PlatformUsbDevice device_;
  const int unique_id_;
  DISALLOW_COPY_AND_ASSIGN(RefCountedPlatformUsbDevice);
};

RefCountedPlatformUsbDevice::RefCountedPlatformUsbDevice(
    PlatformUsbDevice device,
    int unique_id) : device_(device), unique_id_(unique_id) {
  libusb_ref_device(device_);
}

RefCountedPlatformUsbDevice::~RefCountedPlatformUsbDevice() {
  libusb_unref_device(device_);

  // Device is lost.
  // Invalidates all the opened handle.
  for (vector<scoped_refptr<UsbDevice> >::iterator it = handles_.begin();
      it != handles_.end();
      ++it) {
    it->get()->InternalClose();
  }
  STLClearObject(&handles_);
}

scoped_refptr<UsbDevice>
RefCountedPlatformUsbDevice::OpenDevice(UsbService* service) {
  PlatformUsbDeviceHandle handle;
  if (0 == libusb_open(device_, &handle)) {
    scoped_refptr<UsbDevice> wrapper =
        make_scoped_refptr(new UsbDevice(service, unique_id_, handle));
    handles_.push_back(wrapper);
    return wrapper;
  }
  return scoped_refptr<UsbDevice>();
}

void RefCountedPlatformUsbDevice::CloseDevice(UsbDevice* device) {
  device->InternalClose();
  for (vector<scoped_refptr<UsbDevice> >::iterator it = handles_.begin();
        it != handles_.end();
        ++it) {
    if (it->get() == device) {
      handles_.erase(it);
      return;
    }
  }
}

UsbService::UsbService()
    : next_unique_id_(1) {
  libusb_init(&context_);
  event_handler_ = new UsbEventHandler(context_);
}

UsbService::~UsbService() {}

void UsbService::Cleanup() {
  event_handler_->Stop();
  event_handler_ = NULL;
}

void UsbService::FindDevices(const uint16 vendor_id,
                             const uint16 product_id,
                             const int interface_id,
                             vector<int>* devices,
                             const base::Callback<void()>& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  DCHECK(event_handler_) << "FindDevices called after event handler stopped.";
#if defined(OS_CHROMEOS)
  // ChromeOS builds on non-ChromeOS machines (dev) should not attempt to
  // use permission broker.
  if (base::chromeos::IsRunningOnChromeOS()) {
    chromeos::PermissionBrokerClient* client =
        chromeos::DBusThreadManager::Get()->GetPermissionBrokerClient();
    DCHECK(client) << "Could not get permission broker client.";
    if (!client) {
      callback.Run();
      return;
    }

    client->RequestUsbAccess(vendor_id,
                             product_id,
                             interface_id,
                             base::Bind(&UsbService::FindDevicesImpl,
                                        base::Unretained(this),
                                        vendor_id,
                                        product_id,
                                        devices,
                                        callback));
  } else {
    FindDevicesImpl(vendor_id, product_id, devices, callback, true);
  }
#else
  FindDevicesImpl(vendor_id, product_id, devices, callback, true);
#endif  // defined(OS_CHROMEOS)
}

void UsbService::FindDevicesImpl(const uint16 vendor_id,
                                 const uint16 product_id,
                                 vector<int>* devices,
                                 const base::Callback<void()>& callback,
                                 bool success) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  base::ScopedClosureRunner run_callback(callback);

  devices->clear();

  // If the permission broker was unable to obtain permission for the specified
  // devices then there is no point in attempting to enumerate the devices. On
  // platforms without a permission broker, we assume permission is granted.
  if (!success)
    return;

  EnumerateDevices();

  for (DeviceMap::iterator it = devices_.begin();
      it != devices_.end(); ++it) {
    if (DeviceMatches(it->first, vendor_id, product_id)) {
      devices->push_back(it->second->unique_id());
    }
  }
}

void UsbService::OpenDevice(
    int device,
    const base::Callback<void(scoped_refptr<UsbDevice>)>& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  EnumerateDevices();
  for (DeviceMap::iterator it = devices_.begin();
      it != devices_.end(); ++it) {
    if (it->second->unique_id() == device) {
      PlatformUsbDeviceHandle handle;
      if (0 == libusb_open(it->first, &handle)) {
         callback.Run(new UsbDevice(this, device, handle));
         return;
      }
      break;
    }
  }
  callback.Run(NULL);
}

void UsbService::CloseDevice(scoped_refptr<UsbDevice> handle) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  int device = handle->device();

  for (DeviceMap::iterator it = devices_.begin();
      it != devices_.end(); ++it) {
    if (it->second->unique_id() == device) {
      it->second->CloseDevice(handle);
    }
  }
}

void UsbService::ScheduleEnumerateDevice() {
  BrowserThread::PostDelayedTask(
      BrowserThread::FILE,
      FROM_HERE,
      base::Bind(&UsbService::EnumerateDevices, base::Unretained(this)),
      base::TimeDelta::FromSeconds(5));
}

void UsbService::EnumerateDevices() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  libusb_device** devices = NULL;
  const ssize_t device_count = libusb_get_device_list(context_, &devices);
  if (device_count < 0)
    return;

  set<int> connected_devices;
  vector<PlatformUsbDevice> disconnected_devices;

  for (int i = 0; i < device_count; ++i) {
    connected_devices.insert(LookupOrCreateDevice(devices[i]));
  }

  for (DeviceMap::iterator it = devices_.begin();
      it != devices_.end(); ++it) {
    if (!ContainsKey(connected_devices, it->second->unique_id())) {
      disconnected_devices.push_back(it->first);
    }
  }

  for (size_t i = 0; i < disconnected_devices.size(); ++i) {
    // This will delete those devices and invalidate their handles.
    devices_.erase(disconnected_devices[i]);
  }

  libusb_free_device_list(devices, true);
}

bool UsbService::DeviceMatches(PlatformUsbDevice device,
                               const uint16 vendor_id,
                               const uint16 product_id) {
  libusb_device_descriptor descriptor;
  if (libusb_get_device_descriptor(device, &descriptor))
    return false;
  return descriptor.idVendor == vendor_id && descriptor.idProduct == product_id;
}

int UsbService::LookupOrCreateDevice(PlatformUsbDevice device) {
  if (!ContainsKey(devices_, device)) {
    scoped_refptr<RefCountedPlatformUsbDevice> wrapper = make_scoped_refptr(
        new RefCountedPlatformUsbDevice(device, next_unique_id_));
    ++next_unique_id_;
    devices_[device] = wrapper;
  }
  return devices_[device]->unique_id();
}
