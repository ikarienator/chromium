# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'hidapi',
      'type': 'static_library',
      'sources': [
        'src/hidapi/hidapi.h'
      ],
      'include_dirs': [
        'src',
        'src/hidapi',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          'src/hidapi',
        ],
      },
      'conditions': [
        [ 'OS == "linux"', {
          'include_dirs': [
            '../libusb/src/libusb',
          ],
          'sources': [
            'src/libusb/hid.c',
          ],
        }],
        [ 'OS == "mac"', {
          'sources': [
            'src/mac/hid.c',
          ],
        }],
        [ 'OS == "win"', {
          'sources': [
            'src/windows/hid.c',
          ],
          'msvs_disabled_warnings': [ 4267 ],
        }],
      ],
    },
  ],
}
