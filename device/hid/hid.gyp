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
      'conditions': [
        ['OS=="linux"', {
          'dependencies': [
            '../../build/linux/system.gyp:udev',
          ],
        }],
      ],
      'sources': [
        '../../content/browser/udev_linux.cc',
        'hid_connection.cc',
        'hid_connection.h',
        'hid_service.cc',
        'hid_service_linux.cc',
        'hid_service_mac.cc',
        'hid_service.h',
        'hid_utils_mac.cc',
        'hid_utils_mac.h',
      ],
    },
  ],
}
