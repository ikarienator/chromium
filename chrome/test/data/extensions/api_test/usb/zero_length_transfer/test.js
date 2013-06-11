// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var usb = chrome.usb;

var tests = [
  function zeroLengthTransfer() {
    usb.getDevices({vendorId: 0, productId: 0}, function(devices) {
      usb.openDevice(devices[0], function(device) {
        var transfer = new Object();
        transfer.direction = "out";
        transfer.endpoint = 1;
        transfer.data = new ArrayBuffer(0);
        usb.bulkTransfer(device, transfer, function(result) {
          chrome.test.succeed();
        });
      });
    });
  }
];

chrome.test.runTests(tests);
