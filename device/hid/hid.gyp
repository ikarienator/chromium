# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'targets': [
    {
      'target_name': 'device_hid',
      'type': 'static_library',
      'include_dirs': [
        '../..',
      ],
      'dependencies': [
        '../../third_party/hidapi/hidapi.gyp:hidapi',
      ],
      'sources': [
        'hid_connection.cc',
        'hid_connection.h',
        'hid_device.cc',
        'hid_device.h',
        'hid_service.cc',
        'hid_service.h',
      ],
    },
  ],
}
