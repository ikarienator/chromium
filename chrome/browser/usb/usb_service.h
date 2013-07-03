// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_USB_USB_SERVICE_H_
#define CHROME_BROWSER_USB_USB_SERVICE_H_

#include <map>
#include <utility>
#include <vector>

#include "base/basictypes.h"
#include "base/threading/non_thread_safe.h"
#include "base/threading/platform_thread.h"
#include "chrome/browser/usb/usb_device.h"
#include "components/browser_context_keyed_service/browser_context_keyed_service.h"
#include "third_party/libusb/src/libusb/libusb.h"

class UsbContext;
class UsbDeviceStub;
class UsbEventHandler;
class RefCountedPlatformUsbDevice;

typedef libusb_context* PlatformUsbContext;

// The USB service handles creating and managing an event handler thread that is
// used to manage and dispatch USB events. It is also responsbile for device
// discovery on the system, which allows it to re-use device handles to prevent
// competition for the same USB device.
class UsbService : public BrowserContextKeyedService,
                   public base::NonThreadSafe {
 public:
  UsbService();
  virtual ~UsbService();

  // BrowserContextKeyedService:
  virtual void Shutdown() OVERRIDE;

  // Find all of the devices attached to the system that are identified by
  // |vendor_id| and |product_id|, inserting them into |devices|. Clears
  // |devices| before use. Calls |callback| once |devices| is populated.
  void FindDevices(const uint16 vendor_id,
                   const uint16 product_id,
                   int interface_id,
                   std::vector<int>* devices,
                   const base::Callback<void()>& callback);

  // Get all of the devices attached to the system, inserting them into
  // |devices|. Clears |devices| before use.
  void GetDevices(std::vector<int>* devices);

  // Open a device for further communication.
  scoped_refptr<UsbDevice> OpenDevice(int device);

  // This function should not be called by normal code. It is invoked by a
  // UsbDevice's Close function and disposes of the associated platform handle.
  void CloseDevice(scoped_refptr<UsbDevice> device);

  // Schedule an update to USB device info.
  void ScheduleRefreshDevices();

 private:
  // Return true if |device|'s vendor and product identifiers match |vendor_id|
  // and |product_id|.
  static bool DeviceMatches(const UsbDeviceStub* device,
                            const uint16 vendor_id,
                            const uint16 product_id);

  // FindDevicesImpl is called by FindDevices on ChromeOS after the permission
  // broker has signalled that permission has been granted to access the
  // underlying device nodes. On other platforms, it is called directly by
  // FindDevices.
  void FindDevicesImpl(const uint16 vendor_id,
                       const uint16 product_id,
                       std::vector<int>* devices,
                       const base::Callback<void()>& callback,
                       bool success);

  // Enumerate USB devices from OS and Update devices_ map.
  void RefreshDevices();

  scoped_refptr<UsbContext> context_;

  // The next id of device. Can only be accessed from FILE thread.
  int next_unique_id_;

  bool device_enumeration_scheduled_;

  // The devices_ map contains all connected devices.
  // They are not to be used directly outside UsbService. Instead, FindDevice
  // methods returns their id for accessing them.
  typedef std::map<PlatformUsbDevice, UsbDeviceStub*> DeviceMap;
  typedef std::map<int, UsbDeviceStub*> DeviceMapById;
  DeviceMap devices_;
  DeviceMapById devices_by_id_;

  DISALLOW_COPY_AND_ASSIGN(UsbService);
};

#endif  // CHROME_BROWSER_USB_USB_SERVICE_H_
