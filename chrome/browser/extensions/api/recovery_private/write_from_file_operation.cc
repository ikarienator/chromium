// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/time/time.h"
#include "chrome/browser/extensions/api/recovery_private/error_messages.h"
#include "chrome/browser/extensions/api/recovery_private/write_from_file_operation.h"

namespace extensions {
namespace recovery {

WriteFromFileOperation::WriteFromFileOperation(
    RecoveryOperationManager* manager,
    const ExtensionId& extension_id,
    const base::FilePath& path,
    const std::string& storage_unit_id)
  : RecoveryOperation(manager, extension_id, storage_unit_id),
    path_(path) {
}

WriteFromFileOperation::~WriteFromFileOperation() {
}

void WriteFromFileOperation::Start() {
  Error(error::kUnsupportedOperation);
}

} // namespace recovery
} // namespace extensions
