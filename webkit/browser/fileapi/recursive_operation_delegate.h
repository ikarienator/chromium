// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_BROWSER_FILEAPI_RECURSIVE_OPERATION_DELEGATE_H_
#define WEBKIT_BROWSER_FILEAPI_RECURSIVE_OPERATION_DELEGATE_H_

#include <stack>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "webkit/browser/fileapi/file_system_operation.h"
#include "webkit/browser/fileapi/file_system_url.h"

namespace fileapi {

class FileSystemContext;
class FileSystemOperationRunner;

// A base class for recursive operation delegates.
//
// In short, each subclass should override ProcessFile and ProcessDirectory
// to process a directory or a file. To start the recursive operation it
// should also call StartRecursiveOperation.
class RecursiveOperationDelegate
    : public base::SupportsWeakPtr<RecursiveOperationDelegate> {
 public:
  typedef FileSystemOperation::StatusCallback StatusCallback;
  typedef FileSystemOperation::FileEntryList FileEntryList;

  RecursiveOperationDelegate(FileSystemContext* file_system_context);
  virtual ~RecursiveOperationDelegate();

  // This is called when the consumer of this instance starts a non-recursive
  // operation.
  virtual void Run() = 0;

  // This is called when the consumer of this instance starts a recursive
  // operation.
  virtual void RunRecursively() = 0;

  // This is called each time a file is found while recursively
  // performing an operation.
  virtual void ProcessFile(const FileSystemURL& url,
                           const StatusCallback& callback) = 0;

  // This is called each time a directory is found while recursively
  // performing an operation.
  virtual void ProcessDirectory(const FileSystemURL& url,
                                const StatusCallback& callback) = 0;

  // Cancels currently running operations.
  void Cancel();

 protected:
  // Starts to process files/directories recursively from the given |root|.
  // This will call ProcessFile and ProcessDirectory on each directory or file.
  // If the given |root| is a file this simply calls ProcessFile and exits.
  //
  // |callback| is fired with base::PLATFORM_FILE_OK when every file/directory
  // under |root| is processed, or fired earlier when any suboperation fails.
  void StartRecursiveOperation(const FileSystemURL& root,
                               const StatusCallback& callback);

  FileSystemContext* file_system_context() { return file_system_context_; }
  const FileSystemContext* file_system_context() const {
    return file_system_context_;
  }

  FileSystemOperationRunner* operation_runner();

 private:
  void ProcessNextDirectory();
  void ProcessPendingFiles();
  void DidProcessFile(base::PlatformFileError error);
  void DidProcessDirectory(const FileSystemURL& url,
                           base::PlatformFileError error);
  void DidReadDirectory(
      const FileSystemURL& parent,
      base::PlatformFileError error,
      const FileEntryList& entries,
      bool has_more);
  void DidTryProcessFile(base::PlatformFileError previous_error,
                         base::PlatformFileError error);

  // Called when all recursive operation is done (or an error occurs).
  void Done(base::PlatformFileError error);

  FileSystemContext* file_system_context_;
  StatusCallback callback_;
  std::stack<FileSystemURL> pending_directories_;
  std::stack<FileSystemURL> pending_files_;
  int inflight_operations_;
  bool canceled_;

  DISALLOW_COPY_AND_ASSIGN(RecursiveOperationDelegate);
};

}  // namespace fileapi

#endif  // WEBKIT_BROWSER_FILEAPI_RECURSIVE_OPERATION_DELEGATE_H_
