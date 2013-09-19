// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/child_process_logging.h"

#include <windows.h>

#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/crash_keys.h"
#include "chrome/common/metrics/variations/variations_util.h"
#include "chrome/installer/util/google_update_settings.h"

namespace child_process_logging {

namespace {

// exported in breakpad_win.cc: void __declspec(dllexport) __cdecl SetClientId.
typedef void (__cdecl *MainSetClientId)(const wchar_t*);

// exported in breakpad_field_trial_win.cc:
//   void __declspec(dllexport) __cdecl SetExperimentList3
typedef void (__cdecl *MainSetExperimentList)(const wchar_t**, size_t, size_t);

// exported in breakpad_win.cc:
//    void __declspec(dllexport) __cdecl SetCrashKeyValueImpl.
typedef void (__cdecl *SetCrashKeyValue)(const wchar_t*, const wchar_t*);

// exported in breakpad_win.cc:
//    void __declspec(dllexport) __cdecl ClearCrashKeyValueImpl.
typedef void (__cdecl *ClearCrashKeyValue)(const wchar_t*);


// Copied from breakpad_win.cc.
void StringVectorToCStringVector(const std::vector<std::wstring>& wstrings,
                                 std::vector<const wchar_t*>* cstrings) {
  cstrings->clear();
  cstrings->reserve(wstrings.size());
  for (size_t i = 0; i < wstrings.size(); ++i)
    cstrings->push_back(wstrings[i].c_str());
}

}  // namespace

void SetClientId(const std::string& client_id) {
  std::string str(client_id);
  // Remove all instance of '-' char from the GUID. So BCD-WXY becomes BCDWXY.
  ReplaceSubstringsAfterOffset(&str, 0, "-", "");

  if (str.empty())
    return;

  std::wstring wstr = ASCIIToWide(str);
  std::wstring old_wstr;
  if (!GoogleUpdateSettings::GetMetricsId(&old_wstr) ||
      wstr != old_wstr)
    GoogleUpdateSettings::SetMetricsId(wstr);

  static MainSetClientId set_client_id = NULL;
  // note: benign race condition on set_client_id.
  if (!set_client_id) {
    HMODULE exe_module = GetModuleHandle(chrome::kBrowserProcessExecutableName);
    if (!exe_module)
      return;
    set_client_id = reinterpret_cast<MainSetClientId>(
        GetProcAddress(exe_module, "SetClientId"));
    if (!set_client_id)
      return;
  }
  (set_client_id)(wstr.c_str());
}

std::string GetClientId() {
  std::wstring wstr_client_id;
  if (GoogleUpdateSettings::GetMetricsId(&wstr_client_id))
    return WideToASCII(wstr_client_id);
  else
    return std::string();
}

void SetExperimentList(const std::vector<string16>& experiments) {
  static MainSetExperimentList set_experiment_list = NULL;
  // note: benign race condition on set_experiment_list.
  if (!set_experiment_list) {
    HMODULE exe_module = GetModuleHandle(chrome::kBrowserProcessExecutableName);
    if (!exe_module)
      return;
    set_experiment_list = reinterpret_cast<MainSetExperimentList>(
        GetProcAddress(exe_module, "SetExperimentList3"));
    if (!set_experiment_list)
      return;
  }

  std::vector<string16> chunks;
  chrome_variations::GenerateVariationChunks(experiments, &chunks);

  // If the list is empty, notify the child process of the number of experiments
  // and exit early.
  if (chunks.empty()) {
    (set_experiment_list)(NULL, 0, 0);
    return;
  }

  std::vector<const wchar_t*> cstrings;
  StringVectorToCStringVector(chunks, &cstrings);
  (set_experiment_list)(&cstrings[0], cstrings.size(), experiments.size());
}

namespace {

void SetCrashKeyValueTrampoline(const base::StringPiece& key,
                                const base::StringPiece& value) {
  static SetCrashKeyValue set_crash_key = NULL;
  if (!set_crash_key) {
    HMODULE exe_module = GetModuleHandle(chrome::kBrowserProcessExecutableName);
    if (!exe_module)
      return;
    set_crash_key = reinterpret_cast<SetCrashKeyValue>(
        GetProcAddress(exe_module, "SetCrashKeyValueImpl"));
  }

  if (set_crash_key)
    (set_crash_key)(UTF8ToWide(key).data(), UTF8ToWide(value).data());
}

void ClearCrashKeyValueTrampoline(const base::StringPiece& key) {
  static ClearCrashKeyValue clear_crash_key = NULL;
  if (!clear_crash_key) {
    HMODULE exe_module = GetModuleHandle(chrome::kBrowserProcessExecutableName);
    if (!exe_module)
      return;
    clear_crash_key = reinterpret_cast<ClearCrashKeyValue>(
        GetProcAddress(exe_module, "ClearCrashKeyValueImpl"));
  }

  if (clear_crash_key)
    (clear_crash_key)(UTF8ToWide(key).data());
}

}  // namespace

void Init() {
  // Note: on other platforms, this is set up during Breakpad initialization,
  // in ChromeBreakpadClient. But on Windows, that is before the DLL module is
  // loaded, which is a prerequisite of the crash key system.
  crash_keys::RegisterChromeCrashKeys();
  base::debug::SetCrashKeyReportingFunctions(
      &SetCrashKeyValueTrampoline, &ClearCrashKeyValueTrampoline);
}

}  // namespace child_process_logging
