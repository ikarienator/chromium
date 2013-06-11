// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var usb = chrome.usb;

var tests = [
  function listInterfaces() {
    usb.getDevices({vendorId : 0, productId : 0}, function(devices) {
      usb.openDevice(devices[0], function(device) {
        usb.listInterfaces(device, function(result) {
          chrome.test.succeed();
        });
      });
    });
  }
];

chrome.test.runTests(tests);
