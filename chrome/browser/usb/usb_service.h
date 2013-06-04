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
#include "chrome/browser/usb/usb_device.h"
#include "components/browser_context_keyed_service/browser_context_keyed_service.h"
#include "third_party/libusb/src/libusb/libusb.h"

class UsbEventHandler;

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
  void Cleanup();

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
      const base::Callback<void(scoped_refptr<UsbDevice>)>& callback);

  void CloseDevice(scoped_refptr<UsbDevice> handle);

  void ScheduleEnumerateDevice();

 private:
  typedef scoped_refptr<class RefCountedPlatformUsbDevice> PlatformUsbDevicePtr;
  typedef std::vector<PlatformUsbDevicePtr> DeviceVector;

  // Return true if |device|'s vendor and product identifiers match |vendor_id|
  // and |product_id|.
  static bool DeviceMatches(PlatformUsbDevice device,
                            const uint16 vendor_id,
                            const uint16 product_id,
                            const int interface_id);

  // FindDevicesImpl is called by FindDevices on ChromeOS after the permission
  // broker has signalled that permission has been granted to access the
  // underlying device nodes. On other platforms, it is called directly by
  // FindDevices.
  void FindDevicesImpl(const uint16 vendor_id,
                       const uint16 product_id,
                       std::vector<int>* devices,
                       const base::Callback<void()>& callback,
                       bool success);

  // Populates |output| with the result of enumerating all attached USB devices.
  // Must be called from FILE thread.
  void EnumerateDevices();

  // If a UsbDevice wrapper corresponding to |device| has already been created,
  // returns it. Otherwise, opens the device, creates a wrapper, and associates
  // the wrapper with the device internally.
  int LookupOrCreateDevice(PlatformUsbDevice device);

  PlatformUsbContext context_;
  UsbEventHandler* event_handler_;
  int next_unique_id_;

  typedef std::map<PlatformUsbDevice, PlatformUsbDevicePtr> DeviceMap;
  DeviceMap devices_;

  DISALLOW_COPY_AND_ASSIGN(UsbService);
};

#endif  // CHROME_BROWSER_USB_USB_SERVICE_H_
