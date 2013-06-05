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
#include "chrome/browser/usb/usb_device_handle.h"
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

// The UsbEventDispatcher dispatches USB events on separate thread. There is
// currently no way to signal to libusb that any caller into one of the event
// handler calls should return without handling any events.
class UsbEventDispatcher : public base::PlatformThread::Delegate {
 public:
  explicit UsbEventDispatcher(PlatformUsbContext context)
      : running_(true), context_(context) {
    base::PlatformThread::CreateNonJoinable(0, this);
  }

  virtual ~UsbEventDispatcher() {}

  virtual void ThreadMain() OVERRIDE {
    base::PlatformThread::SetName("UsbEventDispatcher");

    DLOG(INFO) << "UsbEventDispatcher started.";
    while (running_) {
      libusb_handle_events(context_);
    }
    DLOG(INFO) << "UsbEventDispatcher shutting down.";
    libusb_exit(context_);

    delete this;
  }

  void Stop() {
    running_ = false;
  }

 private:
  bool running_;
  PlatformUsbContext context_;

  DISALLOW_COPY_AND_ASSIGN(UsbEventDispatcher);
};

// UsbDevice class uniquely represents a USB devices recognized by libusb and
// maintains all its opened handles. It is assigned with an unique id by
// UsbService. Once the device is disconnected it will invalidate all the
// UsbDeviceHandle objects attached to it. The class is only visible to
// UsbService and other classes need to access the device using its unique id.
class UsbDevice : public base::RefCounted<UsbDevice> {
 public:
  explicit UsbDevice(PlatformUsbDevice device,
                     int unique_id);
  PlatformUsbDevice device() const { return device_; }
  int unique_id() const { return unique_id_; }

  scoped_refptr<UsbDeviceHandle> OpenDevice(UsbService* service);
  void CloseDeviceHandle(UsbDeviceHandle* device);

 private:
  virtual ~UsbDevice();
  friend class base::RefCounted<UsbDevice>;
  std::vector<scoped_refptr<UsbDeviceHandle> > handles_;
  PlatformUsbDevice device_;
  const int unique_id_;

  DISALLOW_COPY_AND_ASSIGN(UsbDevice);
};

UsbDevice::UsbDevice(
    PlatformUsbDevice device,
    int unique_id) : device_(device), unique_id_(unique_id) {
  libusb_ref_device(device_);
}

UsbDevice::~UsbDevice() {
  libusb_unref_device(device_);

  // Device is lost.
  // Invalidates all the opened handle.
  for (vector<scoped_refptr<UsbDeviceHandle> >::iterator it = handles_.begin();
      it != handles_.end();
      ++it) {
    it->get()->InternalClose();
  }
  STLClearObject(&handles_);
}

scoped_refptr<UsbDeviceHandle>
UsbDevice::OpenDevice(UsbService* service) {
  PlatformUsbDeviceHandle handle;
  if (0 == libusb_open(device_, &handle)) {
    scoped_refptr<UsbDeviceHandle> wrapper =
        make_scoped_refptr(new UsbDeviceHandle(service, unique_id_, handle));
    handles_.push_back(wrapper);
    return wrapper;
  }
  return scoped_refptr<UsbDeviceHandle>();
}

void UsbDevice::CloseDeviceHandle(UsbDeviceHandle* device) {
  device->InternalClose();
  for (vector<scoped_refptr<UsbDeviceHandle> >::iterator it = handles_.begin();
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
  event_handler_ = new UsbEventDispatcher(context_);
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
    const base::Callback<void(scoped_refptr<UsbDeviceHandle>)>& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  EnumerateDevices();
  for (DeviceMap::iterator it = devices_.begin();
      it != devices_.end(); ++it) {
    if (it->second->unique_id() == device) {
      PlatformUsbDeviceHandle handle;
      if (0 == libusb_open(it->first, &handle)) {
         callback.Run(new UsbDeviceHandle(this, device, handle));
         return;
      }
      break;
    }
  }
  callback.Run(NULL);
}

void UsbService::CloseDeviceHandle(scoped_refptr<UsbDeviceHandle> device,
                             const base::Callback<void()>& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  int id = device->device();

  for (DeviceMap::iterator it = devices_.begin();
      it != devices_.end(); ++it) {
    if (it->second->unique_id() == id) {
      it->second->CloseDeviceHandle(device);
      break;
    }
  }
  callback.Run();
}

void UsbService::ScheduleEnumerateDevice() {
  // TODO(ikarienator): Throttle the schedule.
  BrowserThread::PostTask(
      BrowserThread::FILE,
      FROM_HERE,
      base::Bind(&UsbService::EnumerateDevices, base::Unretained(this)));
}

void UsbService::EnumerateDevices() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  libusb_device** devices = NULL;
  const ssize_t device_count = libusb_get_device_list(context_, &devices);
  if (device_count < 0)
    return;

  set<int> connected_devices;
  vector<PlatformUsbDevice> disconnected_devices;

  // Populates new devices.
  for (int i = 0; i < device_count; ++i) {
    connected_devices.insert(LookupOrCreateDevice(devices[i]));
  }
  libusb_free_device_list(devices, true);

  // Find disconnected devices.
  for (DeviceMap::iterator it = devices_.begin();
      it != devices_.end(); ++it) {
    if (!ContainsKey(connected_devices, it->second->unique_id())) {
      disconnected_devices.push_back(it->first);
    }
  }

  // Remove disconnected devices from devices_.
  for (size_t i = 0; i < disconnected_devices.size(); ++i) {
    // This should delete those devices and invalidate their handles.
    // It might take long.
    devices_.erase(disconnected_devices[i]);
  }
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
    devices_[device] =
        make_scoped_refptr(new UsbDevice(device, next_unique_id_));
    ++next_unique_id_;
  }
  return devices_[device]->unique_id();
}
