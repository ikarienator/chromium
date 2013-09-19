// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/child_process_logging.h"

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

static const size_t kNumSize = 32;
char g_num_variations[kNumSize] = "";

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
