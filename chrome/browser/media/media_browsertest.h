// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_MEDIA_BROWSERTEST_H_
#define CHROME_BROWSER_MEDIA_MEDIA_BROWSERTEST_H_

#include "chrome/test/base/in_process_browser_test.h"


// Class used to automate running media related browser tests. The functions
// assume that media files are located under files/media/ folder known to
// the test http server.
class MediaBrowserTest : public InProcessBrowserTest {
 protected:
  MediaBrowserTest();
  virtual ~MediaBrowserTest();

  static const char kEnded[];
  static const char kError[];
  static const char kFailed[];

  typedef std::pair<const char*, const char*> StringPair;

  // Runs a html page with a list of URL query parameters.
  // If http is true, the test starts a local http test server to load the test
  // page, otherwise a local file URL is loaded inside the content shell.
  // It uses RunTest() to check for expected test output.
  void RunMediaTestPage(const char* html_page,
                        std::vector<StringPair>* query_params,
                        const char* expected, bool http);

  // Opens a URL and waits for the document title to match either one of the
  // default strings or the expected string.
  string16 RunTest(const GURL& gurl, const char* expected);
};

#endif  // CHROME_BROWSER_MEDIA_MEDIA_BROWSERTEST_H_
