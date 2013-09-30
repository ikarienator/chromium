// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_PERFORMANCE_PERFORMANCE_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_PERFORMANCE_PERFORMANCE_API_H_

#include "chrome/browser/extensions/extension_function.h"
#include "chrome/browser/extensions/extension_function_histogram_value.h"

namespace extensions {

class PerformanceGetRequestsExecutionTimeFunction
    : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("permissions.getRequestsExecutionTime",
                             PERFORMANCE_GETREQUESTEXECUTIOTIME)
  PerformanceGetRequestsExecutionTimeFunction();

  virtual bool RunImpl() OVERRIDE;

 private:
   virtual ~PerformanceGetRequestsExecutionTimeFunction();
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_PERFORMANCE_PERFORMANCE_API_H_
