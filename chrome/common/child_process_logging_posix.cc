// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/child_process_logging.h"

#include "base/command_line.h"
#include "base/format_macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/common/metrics/variations/variations_util.h"
#include "chrome/installer/util/google_update_settings.h"

namespace child_process_logging {

// Account for the terminating null character.
static const size_t kClientIdSize = 32 + 1;

// We use static strings to hold the most recent active url and the client
// identifier. If we crash, the crash handler code will send the contents of
// these strings to the browser.
char g_client_id[kClientIdSize];

char g_printer_info[kPrinterInfoStrLen * kMaxReportedPrinterRecords + 1] = "";

static const size_t kNumSize = 32;
char g_num_switches[kNumSize] = "";
char g_num_variations[kNumSize] = "";

// Assume command line switches are less than 64 chars.
static const size_t kMaxSwitchesSize = kSwitchLen * kMaxSwitches + 1;
char g_switches[kMaxSwitchesSize] = "";

static const size_t kMaxVariationChunksSize =
    kMaxVariationChunkSize * kMaxReportedVariationChunks + 1;
char g_variation_chunks[kMaxVariationChunksSize] = "";

void SetClientId(const std::string& client_id) {
  std::string str(client_id);
  ReplaceSubstringsAfterOffset(&str, 0, "-", std::string());

  if (str.empty())
    return;

  base::strlcpy(g_client_id, str.c_str(), kClientIdSize);
  std::wstring wstr = ASCIIToWide(str);
  GoogleUpdateSettings::SetMetricsId(wstr);
}

std::string GetClientId() {
  return std::string(g_client_id);
}

void SetPrinterInfo(const char* printer_info) {
  std::string printer_info_str;
  std::vector<std::string> info;
  base::SplitString(printer_info, L';', &info);
  DCHECK_LE(info.size(), kMaxReportedPrinterRecords);
  for (size_t i = 0; i < info.size(); ++i) {
    printer_info_str += info[i];
    // Truncate long switches, align short ones with spaces to be trimmed later.
    printer_info_str.resize((i + 1) * kPrinterInfoStrLen, ' ');
  }
  base::strlcpy(g_printer_info, printer_info_str.c_str(),
                arraysize(g_printer_info));
}

void SetCommandLine(const CommandLine* command_line) {
  const CommandLine::StringVector& argv = command_line->argv();

  snprintf(g_num_switches, arraysize(g_num_switches), "%" PRIuS,
           argv.size() - 1);

  std::string command_line_str;
  for (size_t argv_i = 1;
       argv_i < argv.size() && argv_i <= kMaxSwitches;
       ++argv_i) {
    command_line_str += argv[argv_i];
    // Truncate long switches, align short ones with spaces to be trimmed later.
    command_line_str.resize(argv_i * kSwitchLen, ' ');
  }
  base::strlcpy(g_switches, command_line_str.c_str(), arraysize(g_switches));
}

void SetExperimentList(const std::vector<string16>& experiments) {
  std::vector<string16> chunks;
  chrome_variations::GenerateVariationChunks(experiments, &chunks);

  // Store up to |kMaxReportedVariationChunks| chunks.
  std::string chunks_str;
  const size_t number_of_chunks_to_report =
      std::min(chunks.size(), kMaxReportedVariationChunks);
  for (size_t i = 0; i < number_of_chunks_to_report; ++i) {
    chunks_str += UTF16ToUTF8(chunks[i]);
    // Align short chunks with spaces to be trimmed later.
    chunks_str.resize(i * kMaxVariationChunkSize, ' ');
  }
  base::strlcpy(g_variation_chunks, chunks_str.c_str(),
                arraysize(g_variation_chunks));

  // Make note of the total number of experiments, which may be greater than
  // what was able to fit in |kMaxReportedVariationChunks|. This is useful when
  // correlating stability with the number of experiments running
  // simultaneously.
  snprintf(g_num_variations, arraysize(g_num_variations), "%" PRIuS,
           experiments.size());
}

}  // namespace child_process_logging
