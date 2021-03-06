// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/indexed_db_factory.h"

#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "content/browser/indexed_db/indexed_db_backing_store.h"
#include "content/browser/indexed_db/indexed_db_tracing.h"
#include "content/browser/indexed_db/indexed_db_transaction_coordinator.h"
#include "third_party/WebKit/public/platform/WebIDBDatabaseException.h"

namespace content {

const int64 kBackingStoreGracePeriodMs = 2000;

static std::string ComputeFileIdentifier(const std::string& origin_identifier) {
  return origin_identifier + "@1";
}

IndexedDBFactory::IndexedDBFactory() {}

IndexedDBFactory::~IndexedDBFactory() {}

void IndexedDBFactory::ReleaseDatabase(
    const IndexedDBDatabase::Identifier& identifier,
    bool forcedClose) {
  DCHECK(database_map_.find(identifier) != database_map_.end());
  std::string backing_store_identifier =
      database_map_[identifier]->backing_store()->identifier();
  database_map_.erase(identifier);

  // No grace period on a forced-close, as the initiator is
  // assuming the backing store will be released once all
  // connections are closed.
  ReleaseBackingStore(backing_store_identifier, forcedClose);
}

void IndexedDBFactory::ReleaseBackingStore(const std::string& identifier,
                                           bool immediate) {
  // Only close if this is the last reference.
  if (!HasLastBackingStoreReference(identifier))
    return;

  if (immediate) {
    CloseBackingStore(identifier);
    return;
  }

  DCHECK(!backing_store_map_[identifier]->close_timer()->IsRunning());
  backing_store_map_[identifier]->close_timer()->Start(
      FROM_HERE,
      base::TimeDelta::FromMilliseconds(kBackingStoreGracePeriodMs),
      base::Bind(&IndexedDBFactory::MaybeCloseBackingStore, this, identifier));
}

void IndexedDBFactory::MaybeCloseBackingStore(const std::string& identifier) {
  // Another reference may have opened since the maybe-close was posted,
  // so it is necessary to check again.
  if (HasLastBackingStoreReference(identifier))
    CloseBackingStore(identifier);
}

void IndexedDBFactory::CloseBackingStore(const std::string& identifier) {
  backing_store_map_.erase(identifier);
}

bool IndexedDBFactory::HasLastBackingStoreReference(
    const std::string& identifier) const {
  IndexedDBBackingStore* ptr;
  {
    // Scope so that the implicit scoped_refptr<> is freed.
    IndexedDBBackingStoreMap::const_iterator it =
        backing_store_map_.find(identifier);
    DCHECK(it != backing_store_map_.end());
    ptr = it->second.get();
  }
  return ptr->HasOneRef();
}

void IndexedDBFactory::GetDatabaseNames(
    scoped_refptr<IndexedDBCallbacks> callbacks,
    const std::string& origin_identifier,
    const base::FilePath& data_directory) {
  IDB_TRACE("IndexedDBFactory::GetDatabaseNames");
  // TODO(dgrogan): Plumb data_loss back to script eventually?
  WebKit::WebIDBCallbacks::DataLoss data_loss;
  scoped_refptr<IndexedDBBackingStore> backing_store =
      OpenBackingStore(origin_identifier, data_directory, &data_loss);
  if (!backing_store) {
    callbacks->OnError(
        IndexedDBDatabaseError(WebKit::WebIDBDatabaseExceptionUnknownError,
                               "Internal error opening backing store for "
                               "indexedDB.webkitGetDatabaseNames."));
    return;
  }

  callbacks->OnSuccess(backing_store->GetDatabaseNames());
}

void IndexedDBFactory::DeleteDatabase(
    const string16& name,
    scoped_refptr<IndexedDBCallbacks> callbacks,
    const std::string& origin_identifier,
    const base::FilePath& data_directory) {
  IDB_TRACE("IndexedDBFactory::DeleteDatabase");
  IndexedDBDatabase::Identifier unique_identifier(origin_identifier, name);
  IndexedDBDatabaseMap::iterator it = database_map_.find(unique_identifier);
  if (it != database_map_.end()) {
    // If there are any connections to the database, directly delete the
    // database.
    it->second->DeleteDatabase(callbacks);
    return;
  }

  // TODO(dgrogan): Plumb data_loss back to script eventually?
  WebKit::WebIDBCallbacks::DataLoss data_loss;
  scoped_refptr<IndexedDBBackingStore> backing_store =
      OpenBackingStore(origin_identifier, data_directory, &data_loss);
  if (!backing_store) {
    callbacks->OnError(
        IndexedDBDatabaseError(WebKit::WebIDBDatabaseExceptionUnknownError,
                               ASCIIToUTF16(
                                   "Internal error opening backing store "
                                   "for indexedDB.deleteDatabase.")));
    return;
  }

  scoped_refptr<IndexedDBDatabase> database =
      IndexedDBDatabase::Create(name, backing_store, this, unique_identifier);
  if (!database) {
    callbacks->OnError(IndexedDBDatabaseError(
        WebKit::WebIDBDatabaseExceptionUnknownError,
        ASCIIToUTF16(
            "Internal error creating database backend for "
            "indexedDB.deleteDatabase.")));
    return;
  }

  database_map_[unique_identifier] = database;
  database->DeleteDatabase(callbacks);
  database_map_.erase(unique_identifier);
}

scoped_refptr<IndexedDBBackingStore> IndexedDBFactory::OpenBackingStore(
    const std::string& origin_identifier,
    const base::FilePath& data_directory,
    WebKit::WebIDBCallbacks::DataLoss* data_loss) {
  const std::string file_identifier = ComputeFileIdentifier(origin_identifier);
  const bool open_in_memory = data_directory.empty();

  IndexedDBBackingStoreMap::iterator it2 =
      backing_store_map_.find(file_identifier);
  if (it2 != backing_store_map_.end()) {
    it2->second->close_timer()->Stop();
    return it2->second;
  }

  scoped_refptr<IndexedDBBackingStore> backing_store;
  if (open_in_memory) {
    backing_store = IndexedDBBackingStore::OpenInMemory(file_identifier);
  } else {
    backing_store = IndexedDBBackingStore::Open(
        origin_identifier, data_directory, file_identifier, data_loss);
  }

  if (backing_store.get()) {
    backing_store_map_[file_identifier] = backing_store;
    // If an in-memory database, bind lifetime to this factory instance.
    if (open_in_memory)
      session_only_backing_stores_.insert(backing_store);

    // All backing stores associated with this factory should be of the same
    // type.
    DCHECK(session_only_backing_stores_.empty() || open_in_memory);

    return backing_store;
  }

  return 0;
}

void IndexedDBFactory::Open(
    const string16& name,
    int64 version,
    int64 transaction_id,
    scoped_refptr<IndexedDBCallbacks> callbacks,
    scoped_refptr<IndexedDBDatabaseCallbacks> database_callbacks,
    const std::string& origin_identifier,
    const base::FilePath& data_directory) {
  IDB_TRACE("IndexedDBFactory::Open");
  scoped_refptr<IndexedDBDatabase> database;
  IndexedDBDatabase::Identifier unique_identifier(origin_identifier, name);
  IndexedDBDatabaseMap::iterator it = database_map_.find(unique_identifier);
  WebKit::WebIDBCallbacks::DataLoss data_loss =
      WebKit::WebIDBCallbacks::DataLossNone;
  if (it == database_map_.end()) {
    scoped_refptr<IndexedDBBackingStore> backing_store =
        OpenBackingStore(origin_identifier, data_directory, &data_loss);
    if (!backing_store) {
      callbacks->OnError(IndexedDBDatabaseError(
          WebKit::WebIDBDatabaseExceptionUnknownError,
          ASCIIToUTF16(
              "Internal error opening backing store for indexedDB.open.")));
      return;
    }

    database =
        IndexedDBDatabase::Create(name, backing_store, this, unique_identifier);
    if (!database) {
      callbacks->OnError(IndexedDBDatabaseError(
          WebKit::WebIDBDatabaseExceptionUnknownError,
          ASCIIToUTF16(
              "Internal error creating database backend for indexedDB.open.")));
      return;
    }

    database_map_[unique_identifier] = database;
  } else {
    database = it->second;
  }

  database->OpenConnection(
      callbacks, database_callbacks, transaction_id, version, data_loss);
}

std::vector<IndexedDBDatabase*> IndexedDBFactory::GetOpenDatabasesForOrigin(
    const std::string& origin_identifier) const {
  std::vector<IndexedDBDatabase*> result;
  for (IndexedDBDatabaseMap::const_iterator it = database_map_.begin();
       it != database_map_.end();
       ++it) {
    if (it->first.first == origin_identifier)
      result.push_back(it->second.get());
  }
  return result;
}

}  // namespace content
