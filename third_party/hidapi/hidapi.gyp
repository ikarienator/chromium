# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'hidapi',
      'type': 'static_library',
      'sources': [
        'hidapi.h',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '.',
        ],
      },
      'conditions': [
        [ 'OS == "win"', {
          'msvs_disabled_warnings': [ 4267 ],
          'sources' : [ 'hidapi_win.c' ],
        }],
        [ 'OS == "mac"', {
          'sources' : [ 'hidapi_mac.c' ],
        }],
        [ 'OS == "linux"', {
          'sources' : [ 'hidapi_linux.c' ],
        }],
      ],
    },
  ],
}
