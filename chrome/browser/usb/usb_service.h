// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_USB_USB_SERVICE_H_
#define CHROME_BROWSER_USB_USB_SERVICE_H_

#include <map>
#include <utility>
#include <vector>

#include "base/basictypes.h"
#include "base/threading/platform_thread.h"
#include "chrome/browser/usb/usb_device_handle.h"
#include "components/browser_context_keyed_service/browser_context_keyed_service.h"
#include "third_party/libusb/src/libusb/libusb.h"

class UsbEventDispatcher;
class UsbDevice;

typedef struct libusb_context* PlatformUsbContext;
typedef struct libusb_device* PlatformUsbDevice;

// The USB service handles creating and managing an event handler thread that is
// used to manage and dispatch USB events. It is also responsbile for device
// discovery on the system, which allows it to re-use device handles to prevent
// competition for the same USB device.
class UsbService : public BrowserContextKeyedService {
 public:
  UsbService();
  virtual ~UsbService();

  // Cleanup must be invoked before the service is destroyed. It interrupts the
  // event handling thread and disposes of open devices.
  virtual void Shutdown() OVERRIDE;

  // Find all of the devices attached to the system that are identified by
  // |vendor_id| and |product_id|, inserting them into |devices|. Clears
  // |devices| before use. Calls |callback| once |devices| is populated.
  void FindDevices(const uint16 vendor_id,
                   const uint16 product_id,
                   const int interface_id,
                   std::vector<int>* devices,
                   const base::Callback<void()>& callback);

  // Open a device for further communication.
  void OpenDevice(
      int device,
      const base::Callback<void(scoped_refptr<UsbDeviceHandle>)>& callback);

  // This function should not be called by normal code. It is invoked by a
  // UsbDevice's Close function and disposes of the associated platform handle.
  void CloseDeviceHandle(scoped_refptr<UsbDeviceHandle> device,
                   const base::Callback<void()>& callback);

  // Schedule an update to USB device info.
  void ScheduleEnumerateDevice();

 private:
  // Return true if |device|'s vendor and product identifiers match |vendor_id|
  // and |product_id|.
  static bool DeviceMatches(PlatformUsbDevice device,
                            const uint16 vendor_id,
                            const uint16 product_id);

  // FindDevicesImpl is called by FindDevices on ChromeOS after the permission
  // broker has signaled that permission has been granted to access the
  // underlying device nodes. On other platforms, it is called directly by
  // FindDevices.
  void FindDevicesImpl(const uint16 vendor_id,
                       const uint16 product_id,
                       std::vector<int>* devices,
                       const base::Callback<void()>& callback,
                       bool success);

  // Enumerate USB devices from OS and Update devices_ map.
  void EnumerateDevices();

  // If a UsbDevice wrapper corresponding to |device| has already been created,
  // returns its unique id. Otherwise, creates a wrapper and associates it with
  // the device and the next unique id.
  int LookupOrCreateDevice(PlatformUsbDevice device);

  PlatformUsbContext context_;
  UsbEventDispatcher* event_dispatcher_;
  int next_unique_id_;

  // The devices_ map contains scoped_refptrs to all connected devices.
  // They are intended to be used directly outside UsbService. Instead,
  // FindDevice methods returns their id for accessing them.
  typedef std::map<PlatformUsbDevice, scoped_refptr<UsbDevice> > DeviceMap;
  DeviceMap devices_;

  DISALLOW_COPY_AND_ASSIGN(UsbService);
};

#endif  // CHROME_BROWSER_USB_USB_SERVICE_H_
