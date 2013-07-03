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
#include "third_party/libusb/src/libusb/interrupt.h"
#include "third_party/libusb/src/libusb/libusb.h"

#if defined(OS_CHROMEOS)
#include "base/chromeos/chromeos_version.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/permission_broker_client.h"
#endif  // defined(OS_CHROMEOS)

using std::set;
using std::vector;
using content::BrowserThread;

// The UsbEventHandler dispatches USB events on separate thread. There is
// currently no way to signal to libusb that any caller into one of the event
// handler calls should return without handling any events.
//
// This class manages the polling thread and assures the thread exits safely.
// This class is only visible to UsbContext. UsbContext manages its life cycle.
class UsbEventHandler : public base::PlatformThread::Delegate {
 public:
  explicit UsbEventHandler(PlatformUsbContext context)
      : running_(true), context_(context), thread_handle_(0) {
    base::PlatformThread::Create(0, this, &thread_handle_);
  }

  virtual ~UsbEventHandler() {}

  virtual void ThreadMain() OVERRIDE {
    base::PlatformThread::SetName("UsbEventHandler");

    VLOG(1) << "UsbEventHandler started.";

    while (running_)
      libusb_handle_events(context_);

    VLOG(1) << "UsbEventHandler shutting down.";
  }

  void Stop() {
    running_ = false;
    base::subtle::MemoryBarrier();
    libusb_interrupt_handle_event(context_);
    base::PlatformThread::Join(thread_handle_);
  }

 private:
  volatile bool running_;
  PlatformUsbContext context_;
  base::PlatformThreadHandle thread_handle_;

  DISALLOW_COPY_AND_ASSIGN(UsbEventHandler);
};

// Ref-counted wrapper for PlatformUsbContext.
// It also manages the life-cycle of UsbEventHandler
class UsbContext : public base::RefCountedThreadSafe<UsbContext> {
 public:
  UsbContext() : context_(NULL) {
    CHECK_EQ(0, libusb_init(&context_)) << "Cannot initialize libusb";
    event_handler_.reset(new UsbEventHandler(context_));
  }
  PlatformUsbContext context() const { return context_; }

 private:
  friend class base::RefCountedThreadSafe<UsbContext>;

  virtual ~UsbContext() {
    event_handler_->Stop();
    event_handler_.reset(NULL);
    libusb_exit(context_);
  }
  PlatformUsbContext context_;
  scoped_ptr<UsbEventHandler> event_handler_;
};

class UsbDeviceStub : public base::NonThreadSafe {
 public:
  explicit UsbDeviceStub(UsbContext* context, PlatformUsbDevice device,
                     const int unique_id, const uint16 vendor_id,
                     const uint16 product_id)
      : context_(context),
        device_(device),
        unique_id_(unique_id),
        vendor_id_(vendor_id),
        product_id_(product_id) {
    DCHECK(CalledOnValidThread());
    libusb_ref_device(device_);
  }

  virtual ~UsbDeviceStub() {
    DCHECK(CalledOnValidThread());
    libusb_unref_device(device_);

    // Device is lost.
    // Invalidates all the opened handle.
    for (vector<scoped_refptr<UsbDevice> >::iterator it = handles_.begin();
         it != handles_.end(); ++it) {
      it->get()->InternalClose();
    }
    STLClearObject(&handles_);
  }

  PlatformUsbDevice device() const { return device_; }
  int unique_id() const { return unique_id_; }
  int vendor_id() const { return vendor_id_; }
  int product_id() const { return product_id_; }

  scoped_refptr<UsbDevice> OpenDevice(UsbService* service) {
    DCHECK(CalledOnValidThread());
    PlatformUsbDeviceHandle handle;

    if (0 == libusb_open(device_, &handle)) {
      scoped_refptr<UsbDevice> wrapper =
          make_scoped_refptr(new UsbDevice(
              service,
              unique_id_,
              handle));
      handles_.push_back(wrapper);
      return wrapper;
    }
    return scoped_refptr<UsbDevice>();
  }

  void CloseDeviceHandle(UsbDevice* device) {
    DCHECK(CalledOnValidThread());
    device->InternalClose();
    for (vector<scoped_refptr<UsbDevice> >::iterator it = handles_.begin();
        it != handles_.end(); ++it) {
      if (it->get() == device) {
        handles_.erase(it);
        return;
      }
    }
  }

 private:
  // Retain the context so it will not be release before the destruction
  // of the UsbDevice object.
  scoped_refptr<UsbContext> context_;
  vector<scoped_refptr<UsbDevice> > handles_;
  const PlatformUsbDevice device_;
  const uint16 unique_id_;
  const uint16 vendor_id_;
  const int product_id_;
  DISALLOW_COPY_AND_ASSIGN(UsbDeviceStub);
};

UsbService::UsbService()
    : context_(new UsbContext()),
      next_unique_id_(1),
      device_enumeration_scheduled_(false) {
  // This class will be consequently used on FILE thread.
  DetachFromThread();
}

UsbService::~UsbService() {
  // The destructor will be called on UI thread.
  DetachFromThread();
}

void UsbService::Shutdown() {
  context_ = NULL;
  for (DeviceMapById::iterator it = devices_by_id_.begin();
      it != devices_by_id_.end(); ++it) {
    BrowserThread::DeleteSoon(BrowserThread::FILE, FROM_HERE, it->second);
  }
  devices_.clear();
  devices_by_id_.clear();
}

void UsbService::FindDevices(const uint16 vendor_id,
                             const uint16 product_id,
                             int interface_id,
                             vector<int>* devices,
                             const base::Callback<void()>& callback) {
  DCHECK(CalledOnValidThread());
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

scoped_refptr<UsbDevice> UsbService::OpenDevice(int device) {
  DCHECK(CalledOnValidThread());
  RefreshDevices();
  if (ContainsKey(devices_by_id_, device)) {
    return devices_by_id_[device]->OpenDevice(this);
  }
  return NULL;
}

void UsbService::CloseDevice(scoped_refptr<UsbDevice> device) {
  DCHECK(CalledOnValidThread());
  if (ContainsKey(devices_by_id_, device->device())) {
    return devices_by_id_[device->device()]->CloseDeviceHandle(device);
  }
}

void UsbService::GetDevices(std::vector<int>* devices) {
  DCHECK(CalledOnValidThread());
  devices->clear();
  RefreshDevices();
  for (DeviceMapById::iterator it = devices_by_id_.begin();
      it != devices_by_id_.end();
      ++it) {
    devices->push_back(it->first);
  }
}

void UsbService::FindDevicesImpl(const uint16 vendor_id,
                                 const uint16 product_id,
                                 vector<int>* devices,
                                 const base::Callback<void()>& callback,
                                 bool success) {
  DCHECK(CalledOnValidThread());
  base::ScopedClosureRunner run_callback(callback);
  devices->clear();

  // If the permission broker was unable to obtain permission for the specified
  // devices then there is no point in attempting to enumerate the devices. On
  // platforms without a permission broker, we assume permission is granted.
  if (!success)
    return;

  RefreshDevices();

  for (DeviceMapById::iterator it = devices_by_id_.begin();
      it != devices_by_id_.end();
      ++it) {
    if (DeviceMatches(it->second, vendor_id, product_id))
      devices->push_back(it->first);
  }
}

void UsbService::ScheduleRefreshDevices() {
  DCHECK(CalledOnValidThread());
  if (device_enumeration_scheduled_)
    return;
  device_enumeration_scheduled_ = true;
  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(&UsbService::RefreshDevices, base::Unretained(this)));
}

void UsbService::RefreshDevices() {
  DCHECK(CalledOnValidThread());
  libusb_device** devices = NULL;
  const ssize_t device_count =
      libusb_get_device_list(context_->context(), &devices);

  set<int> connected_devices;
  vector<PlatformUsbDevice> disconnected_devices;

  // Populates new devices.
  for (ssize_t i = 0; i < device_count; ++i) {
    if (!ContainsKey(devices_, devices[i])) {
      libusb_device_descriptor descriptor;
      if (0 != libusb_get_device_descriptor(devices[i], &descriptor))
        continue;
      devices_[devices[i]] = new UsbDeviceStub(
          context_.get(),
          devices[i],
          next_unique_id_,
          descriptor.idVendor,
          descriptor.idProduct);
      devices_by_id_[next_unique_id_] = devices_[devices[i]];
      ++next_unique_id_;
    }
    connected_devices.insert(devices_[devices[i]]->unique_id());
  }

  // Find disconnected devices.
  for (DeviceMap::iterator it = devices_.begin(); it != devices_.end(); ++it) {
    if (!ContainsKey(connected_devices, it->second->unique_id())) {
      disconnected_devices.push_back(it->first);
    }
  }

  // Remove disconnected devices from devices_.
  for (size_t i = 0; i < disconnected_devices.size(); ++i) {
    UsbDeviceStub* device = devices_[disconnected_devices[i]];
    devices_.erase(disconnected_devices[i]);
    devices_by_id_.erase(device->unique_id());

    // This should delete those devices and invalidate their handles.
    // It might take long.
    delete device;
  }

  libusb_free_device_list(devices, true);
}

bool UsbService::DeviceMatches(const UsbDeviceStub* device,
                               const uint16 vendor_id,
                               const uint16 product_id) {
  return device->vendor_id() == vendor_id && device->product_id() == product_id;
}
