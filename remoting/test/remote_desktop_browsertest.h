// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTE_DESKTOP_BROWSER_TEST_H_
#define REMOTE_DESKTOP_BROWSER_TEST_H_

#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/notification_service.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"

namespace {
// Command line arguments specific to the chromoting browser tests.
const char kOverrideUserDataDir[] = "override-user-data-dir";
const char kNoCleanup[] = "no-cleanup";
const char kNoInstall[] = "no-install";
const char kWebAppCrx[] = "webapp-crx";
const char kUsername[] = "username";
const char kkPassword[] = "password";
const char kMe2MePin[] = "me2me-pin";

// ASSERT_TRUE can only be used in void returning functions. This version
// should be used in non-void-returning functions.
inline void _ASSERT_TRUE(bool condition) {
  ASSERT_TRUE(condition);
  return;
}

}

namespace remoting {

class RemoteDesktopBrowserTest : public ExtensionBrowserTest {
 public:
  RemoteDesktopBrowserTest();
  virtual ~RemoteDesktopBrowserTest();

  // InProcessBrowserTest Overrides
  virtual void SetUp() OVERRIDE;

 protected:
  // InProcessBrowserTest Overrides
  virtual void SetUpInProcessBrowserTestFixture() OVERRIDE;

  // InProcessBrowserTest Overrides
  virtual void TearDownInProcessBrowserTestFixture() OVERRIDE;

  // The following helpers each perform a simple task.

  // Verify the test has access to the internet (specifically google.com)
  void VerifyInternetAccess();

  // Install the chromoting extension from a crx file.
  void InstallChromotingApp();

  // Uninstall the chromoting extension.
  void UninstallChromotingApp();

  // Test whether the chromoting extension is installed.
  void VerifyChromotingLoaded(bool expected);

  // Launch the chromoting app.
  void LaunchChromotingApp();

  // Authorize: grant extended access permission to the user's computer.
  void Authorize();

  // Authenticate: sign in to google using the credentials provided.
  void Authenticate();

  // Approve: grant the chromoting app necessary permissions.
  void Approve();

  // Click on "Get Started" in the Me2Me section and show the host list.
  void StartMe2Me();

  // Simulate a key event.
  void SimulateKeyPressWithCode(ui::KeyboardCode keyCode, const char* code);

  void SimulateKeyPressWithCode(ui::KeyboardCode keyCode,
                                const char* code,
                                bool control,
                                bool shift,
                                bool alt,
                                bool command);

  // The following helpers each perform a composite task.

  // Install the chromoting extension
  void Install();

  // Clean up after the test.
  void Cleanup();

  // Perform all the auth steps: authorization, authenticattion, etc.
  // It starts from the chromoting main page unauthenticated and ends up back
  // on the chromoting main page authenticated and ready to go.
  void Auth();

  // Connect to the local host through Me2Me.
  void ConnectToLocalHost();

  // Enter the pin number and connect.
  void EnterPin(const std::string& name);

  // Helper to get the pin number used for me2me authentication.
  std::string me2me_pin() { return me2me_pin_; }

 private:
  // Change behavior of the default host resolver to allow DNS lookup
  // to proceed instead of being blocked by the test infrastructure.
  void EnableDNSLookupForThisTest(
    net::RuleBasedHostResolverProc* host_resolver);

  // We need to reset the DNS lookup when we finish, or the test will fail.
  void DisableDNSLookupForThisTest();

  void ParseCommandLine();

  // Accessor methods.

  // Helper to get the path to the crx file of the webapp to be tested.
  base::FilePath WebAppCrxPath() { return webapp_crx_; }

  // Helper to get the extension ID of the installed chromoting webapp.
  std::string ChromotingID() { return chromoting_id_; }

  // Whether to perform the cleanup tasks (uninstalling chromoting, etc).
  // This is useful for diagnostic purposes.
  bool NoCleanup() { return no_cleanup_; }

  // Whether to install the chromoting extension before running the test cases.
  // This is useful for diagnostic purposes.
  bool NoInstall() { return no_install_; }

  // Helper to construct the starting URL of the installed chromoting webapp.
  GURL Chromoting_Main_URL() {
    return GURL("chrome-extension://" + ChromotingID() + "/main.html");
  }

  // Helper to retrieve the current URL of the active tab in the browser.
  GURL GetCurrentURL() {
    return browser()->tab_strip_model()->GetActiveWebContents()->GetURL();
  }

  // Helpers to execute javascript code on a web page.

  // Helper to execute a javascript code snippet on the current page.
  void ExecuteScript(const std::string& script);

  // Helper to execute a javascript code snippet on the current page and
  // wait for page load to complete.
  void ExecuteScriptAndWaitForAnyPageLoad(const std::string& script);

  // Helper to execute a javascript code snippet on the current page and
  // wait until the target url is loaded. This is used when the target page
  // is loaded after multiple redirections.
  void ExecuteScriptAndWaitForPageLoad(const std::string& script,
                                       const GURL& target);

  // Helper to execute a javascript code snippet on the current page and
  // extract the boolean result.
  bool ExecuteScriptAndExtractBool(const std::string& script);

  // Helper to execute a javascript code snippet on the current page and
  // extract the int result.
  int ExecuteScriptAndExtractInt(const std::string& script);

  // Helper to execute a javascript code snippet on the current page and
  // extract the string result.
  std::string ExecuteScriptAndExtractString(const std::string& script);

  // Helper to navigate to a given url.
  void NavigateToURLAndWaitForPageLoad(const GURL& url);

  // Helper to check whether an html element with the given name exists on
  // the current page.
  bool HtmlElementExists(const std::string& name) {
    return ExecuteScriptAndExtractBool(
        "document.getElementById(\"" + name + "\") != null");
  }

  // Helper to check whether a html element with the given name is visible.
  bool HtmlElementVisible(const std::string& name);

  // Click on the named HTML control.
  void ClickOnControl(const std::string& name);

  // Wait for the me2me connection to be established.
  void WaitForConnection();

  // Checking whether the localHost has been initialized.
  bool IsLocalHostReady();

  // Callback used by EnterPin to check whether the pin form is visible
  // and to dismiss the host-needs-update dialog.
  bool IsPinFormVisible();

  // Callback used by WaitForConnection to check whether the connection
  // has been established.
  bool IsSessionConnected();

  // Callback used by ExecuteScriptAndWaitForPageLoad to check whether
  // the given page is currently loaded.
  bool IsURLLoaded(const GURL& url);

  bool RetrieveRedirectURL();

  // Fields

  // This test needs to make live DNS requests for access to
  // GAIA and sync server URLs under google.com. We use a scoped version
  // to override the default resolver while the test is active.
  scoped_ptr<net::ScopedDefaultHostResolverProc> mock_host_resolver_override_;

  bool no_cleanup_;
  bool no_install_;
  std::string chromoting_id_;
  base::FilePath webapp_crx_;
  std::string username_;
  std::string password_;
  std::string me2me_pin_;

  // TODO: Remove this when issue 291207 is fixed.
  // http://crbug.com/294343
  std::string oauth_redirect_url_;
};

}  // namespace remoting

#endif  // REMOTE_DESKTOP_BROWSER_TEST_H_
