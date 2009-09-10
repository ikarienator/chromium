// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This used to do a lot of TLS-based management of multiple Directory objects.
// We now can access Directory objects from any thread for general purpose
// operations and we only ever have one Directory, so this class isn't doing
// anything too fancy besides keeping calling and access conventions the same
// for now.
// TODO(timsteele): We can probably nuke this entire class and use raw
// Directory objects everywhere.
#ifndef CHROME_BROWSER_SYNC_SYNCABLE_DIRECTORY_MANAGER_H_
#define CHROME_BROWSER_SYNC_SYNCABLE_DIRECTORY_MANAGER_H_

#include <pthread.h>

#include <vector>

#include "base/atomicops.h"
#include "base/basictypes.h"
#include "chrome/browser/sync/syncable/dir_open_result.h"
#include "chrome/browser/sync/syncable/path_name_cmp.h"
#include "chrome/browser/sync/syncable/syncable.h"
#include "chrome/browser/sync/util/event_sys.h"
#include "chrome/browser/sync/util/sync_types.h"

namespace sync_api { class BaseTransaction; }

namespace syncable {

struct DirectoryManagerEvent {
  enum {
    OPEN_FAILED,
    OPENED,
    CLOSED,
    CLOSED_ALL,
    SHUTDOWN,
  } what_happened;
  PathString dirname;
  DirOpenResult error;  // Only for OPEN_FAILED.
  typedef DirectoryManagerEvent EventType;
  static inline bool IsChannelShutdownEvent(const EventType& event) {
    return SHUTDOWN == event.what_happened;
  }
};

DirectoryManagerEvent DirectoryManagerShutdownEvent();

class DirectoryManager {
 public:
  typedef EventChannel<DirectoryManagerEvent> Channel;

  // root_path specifies where db is stored.
  explicit DirectoryManager(const PathString& root_path);
  ~DirectoryManager();

  static const PathString GetSyncDataDatabaseFilename();
  const PathString GetSyncDataDatabasePath() const;

  // Opens a directory.  Returns false on error.
  // Name parameter is the the user's login,
  // MUST already have been converted to a common case.
  bool Open(const PathString& name);

  // Marks a directory as closed.  It might take a while until all the
  // file handles and resources are freed by other threads.
  void Close(const PathString& name);

  // Marks all directories as closed.  It might take a while until all the
  // file handles and resources are freed by other threads.
  void CloseAllDirectories();

  // Should be called at App exit.
  void FinalSaveChangesForAll();

  // Gets the list of currently open directory names.
  typedef std::vector<PathString> DirNames;
  void GetOpenDirectories(DirNames* result);

  Channel* channel() const { return channel_; }

 protected:
  DirOpenResult OpenImpl(const PathString& name, const PathString& path,
                         bool* was_open);

  // Helpers for friend class ScopedDirLookup:
  friend class ScopedDirLookup;

  const PathString root_path_;
  // protects managed_directory_
  mutable pthread_mutex_t mutex_;
  Directory* managed_directory_;

  Channel* const channel_;

 private:

  DISALLOW_COPY_AND_ASSIGN(DirectoryManager);
};


class ScopedDirLookup {
 public:
  ScopedDirLookup(DirectoryManager* dirman, const PathString& name);
  ~ScopedDirLookup();

  inline bool good() {
    good_checked_ = true;
    return good_;
  }
  Directory* operator -> () const;
  operator Directory* () const;

 protected:  // Don't allow creation on heap, except by sync API wrapper.
  friend class sync_api::BaseTransaction;
  void* operator new(size_t size) { return (::operator new)(size); }

  Directory* dir_;
  bool good_;
  // Ensure that the programmer checks good before using the ScopedDirLookup
  // This member should can be removed if it ever shows up in profiling
  bool good_checked_;
  DirectoryManager* const dirman_;
};

}  // namespace syncable

#endif  // CHROME_BROWSER_SYNC_SYNCABLE_DIRECTORY_MANAGER_H_
