// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var usb = chrome.usb;

var tests = [
  function explicitCloseDevice() {
    usb.getDevices({vendorId: 0, productId: 0}, function(devices) {
      usb.openDevice(devices[0], function(device) {
        usb.closeDevice(device, function() {
          chrome.test.assertEq(undefined, chrome.runtime.lastError);
          chrome.test.succeed();
        });
      });
    });
  },
  function resetDevice() {
    usb.getDevices({vendorId: 0, productId: 0}, function(devices) {
      usb.openDevice(devices[0], function(device) {
        usb.resetDevice(device, function(result) {
          chrome.test.assertEq(result, true);
          // Ensure the device is still open.
          var transfer = {direction: "out", endpoint: 2,
                          data: new ArrayBuffer(1)};
          usb.interruptTransfer(device, transfer, function(result) {
              // This is designed to fail.
            usb.resetDevice(device, function(result) {
              chrome.test.assertEq(result, false);
              usb.interruptTransfer(device, transfer, function(result) {
                chrome.test.assertEq(result, undefined);
                chrome.test.assertEq(
                    chrome.runtime.lastError &&
                    chrome.runtime.lastError.message,
                    'No such device.');
                chrome.test.succeed();
              });
            });
          });
        });
      });
    });
  },
];

chrome.test.runTests(tests);
