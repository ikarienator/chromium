// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/performance/performance_api.h"

namespace extensions {

PerformanceGetRequestsExecutionTimeFunction::
    PerformanceGetRequestsExecutionTimeFunction() {}

PerformanceGetRequestsExecutionTimeFunction::
    ~PerformanceGetRequestsExecutionTimeFunction() {}

bool PerformanceGetRequestsExecutionTimeFunction::RunImpl() {
  return false;
}

}  // namespace extensions
