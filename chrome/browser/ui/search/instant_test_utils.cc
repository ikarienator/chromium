// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/search/instant_test_utils.h"

#include "base/command_line.h"
#include "base/prefs/pref_service.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/instant_service.h"
#include "chrome/browser/search/instant_service_factory.h"
#include "chrome/browser/search_engines/template_url_service.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/omnibox/omnibox_view.h"
#include "chrome/browser/ui/search/instant_ntp.h"
#include "chrome/browser/ui/search/instant_ntp_prerenderer.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/result_codes.h"
#include "content/public/test/browser_test_utils.h"

namespace {

std::string WrapScript(const std::string& script) {
  return "domAutomationController.send(" + script + ")";
}

}  // namespace

// InstantTestBase -----------------------------------------------------------

void InstantTestBase::SetupInstant(Browser* browser) {
  browser_ = browser;
  TemplateURLService* service =
      TemplateURLServiceFactory::GetForProfile(browser_->profile());
  ui_test_utils::WaitForTemplateURLServiceToLoad(service);

  TemplateURLData data;
  // Necessary to use exact URL for both the main URL and the alternate URL for
  // search term extraction to work in InstantExtended.
  data.short_name = ASCIIToUTF16("name");
  data.SetURL(instant_url_.spec() +
              "q={searchTerms}&is_search&{google:omniboxStartMarginParameter}");
  data.instant_url = instant_url_.spec();
  if (init_suggestions_url_)
    data.suggestions_url = instant_url_.spec() + "#q={searchTerms}";
  data.alternate_urls.push_back(instant_url_.spec() + "#q={searchTerms}");
  data.search_terms_replacement_key = "strk";

  TemplateURL* template_url = new TemplateURL(browser_->profile(), data);
  service->Add(template_url);  // Takes ownership of |template_url|.
  service->SetDefaultSearchProvider(template_url);

  InstantService* instant_service =
      InstantServiceFactory::GetForProfile(browser_->profile());
  ASSERT_NE(static_cast<InstantService*>(NULL), instant_service);
  instant_service->ntp_prerenderer()->ReloadStaleNTP();
}

void InstantTestBase::SetInstantURL(const std::string& url) {
  TemplateURLService* service =
      TemplateURLServiceFactory::GetForProfile(browser_->profile());
  ui_test_utils::WaitForTemplateURLServiceToLoad(service);

  TemplateURLData data;
  data.short_name = ASCIIToUTF16("name");
  data.SetURL(url);
  data.instant_url = url;

  TemplateURL* template_url = new TemplateURL(browser_->profile(), data);
  service->Add(template_url);  // Takes ownership of |template_url|.
  service->SetDefaultSearchProvider(template_url);
}

void InstantTestBase::Init(const GURL& instant_url, bool init_suggestions_url) {
  instant_url_ = instant_url;
  init_suggestions_url_ = init_suggestions_url;
}

void InstantTestBase::FocusOmnibox() {
  // If the omnibox already has focus, just notify Instant.
  if (omnibox()->model()->has_focus()) {
    instant()->OmniboxFocusChanged(OMNIBOX_FOCUS_VISIBLE,
                                   OMNIBOX_FOCUS_CHANGE_EXPLICIT, NULL);
  } else {
    browser_->window()->GetLocationBar()->FocusLocation(false);
  }
}

void InstantTestBase::FocusOmniboxAndWaitForInstantNTPSupport() {
  content::WindowedNotificationObserver ntp_observer(
      chrome::NOTIFICATION_INSTANT_NTP_SUPPORT_DETERMINED,
      content::NotificationService::AllSources());
  FocusOmnibox();

  InstantService* instant_service =
      InstantServiceFactory::GetForProfile(browser_->profile());
  ASSERT_NE(static_cast<InstantService*>(NULL), instant_service);
  if (!instant_service->ntp_prerenderer()->ntp() ||
      !instant_service->ntp_prerenderer()->ntp()->supports_instant())
    ntp_observer.Wait();
}

void InstantTestBase::SetOmniboxText(const std::string& text) {
  FocusOmnibox();
  omnibox()->SetUserText(UTF8ToUTF16(text));
}

void InstantTestBase::PressEnterAndWaitForNavigation() {
  content::WindowedNotificationObserver nav_observer(
      content::NOTIFICATION_NAV_ENTRY_COMMITTED,
      content::NotificationService::AllSources());
  browser_->window()->GetLocationBar()->AcceptInput();
  nav_observer.Wait();
}

bool InstantTestBase::GetBoolFromJS(content::WebContents* contents,
                                    const std::string& script,
                                    bool* result) {
  return content::ExecuteScriptAndExtractBool(
      contents, WrapScript(script), result);
}

bool InstantTestBase::GetIntFromJS(content::WebContents* contents,
                                   const std::string& script,
                                   int* result) {
  return content::ExecuteScriptAndExtractInt(
      contents, WrapScript(script), result);
}

bool InstantTestBase::GetStringFromJS(content::WebContents* contents,
                                      const std::string& script,
                                      std::string* result) {
  return content::ExecuteScriptAndExtractString(
      contents, WrapScript(script), result);
}

bool InstantTestBase::ExecuteScript(const std::string& script) {
  InstantService* instant_service =
      InstantServiceFactory::GetForProfile(browser_instant()->profile());
  if (!instant_service)
    return false;
  return content::ExecuteScript(instant_service->GetNTPContents(), script);
}

bool InstantTestBase::CheckVisibilityIs(content::WebContents* contents,
                                        bool expected) {
  bool actual = !expected;  // Purposely start with a mis-match.
  // We can only use ASSERT_*() in a method that returns void, hence this
  // convoluted check.
  return GetBoolFromJS(contents, "!document.webkitHidden", &actual) &&
      actual == expected;
}

std::string InstantTestBase::GetOmniboxText() {
  return UTF16ToUTF8(omnibox()->GetText());
}

bool InstantTestBase::LoadImage(content::RenderViewHost* rvh,
                                const std::string& image,
                                bool* loaded) {
  std::string js_chrome =
      "var img = document.createElement('img');"
      "img.onerror = function() { domAutomationController.send(false); };"
      "img.onload  = function() { domAutomationController.send(true); };"
      "img.src = '" + image + "';";
  return content::ExecuteScriptAndExtractBool(rvh, js_chrome, loaded);
}

string16 InstantTestBase::GetBlueText() {
  size_t start = 0, end = 0;
  omnibox()->GetSelectionBounds(&start, &end);
  if (start > end)
    std::swap(start, end);
  return omnibox()->GetText().substr(start, end - start);
}
