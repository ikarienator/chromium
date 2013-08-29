# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'targets': [
    {
      'target_name': 'device_usb',
      'type': 'static_library',
      'include_dirs': [
        '../..',
      ],
      'dependencies': [
        '../../third_party/libusb/libusb.gyp:libusb',
      ],
      'sources': [
        'usb_context.cc',
        'usb_context.h',
        'usb_device_handle.cc',
        'usb_device_handle.h',
        'usb_device.cc',
        'usb_device.h',
        'usb_device.cc',
        'usb_ids.cc',
        'usb_ids.h',
        'usb_interface.cc',
        'usb_interface.h',
        'usb_service.cc',
        'usb_service.h',
      ],
      'actions': [
        {
          'action_name': 'generate_usb_ids',
          'variables': {
            'usb_ids_path%': '<(DEPTH)/third_party/usb_ids/usb.ids',
            'usb_ids_py_path': '<(DEPTH)/tools/usb_ids/usb_ids.py',
          },
          'inputs': [
            '<(usb_ids_path)',
            '<(usb_ids_py_path)',
          ],
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/device/usb/usb_ids_gen.cc',
          ],
          'action': [
            'python',
            '<(usb_ids_py_path)',
            '-i', '<(usb_ids_path)',
            '-o', '<@(_outputs)',
          ],
          'process_outputs_as_sources': 1,
        },
      ],
    },
  ],
}
