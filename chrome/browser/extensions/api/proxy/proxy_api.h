// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Defines the Chrome Extensions Proxy Settings API relevant classes to realize
// the API as specified in the extension API JSON.

#ifndef CHROME_BROWSER_EXTENSIONS_API_PROXY_PROXY_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_PROXY_PROXY_API_H_

#include <string>

#include "base/memory/singleton.h"
#include "base/strings/string16.h"
#include "chrome/browser/extensions/api/preference/preference_api.h"
#include "chrome/browser/prefs/proxy_prefs.h"

namespace base {
class Value;
}

namespace extensions {
class EventRouterForwarder;

// Class to convert between the representation of proxy settings used
// in the Proxy Settings API and the representation used in the PrefStores.
// This plugs into the ExtensionPreferenceAPI to get and set proxy settings.
class ProxyPrefTransformer : public PrefTransformerInterface {
 public:
  ProxyPrefTransformer();
  virtual ~ProxyPrefTransformer();

  // Implementation of PrefTransformerInterface.
  virtual base::Value* ExtensionToBrowserPref(const base::Value* extension_pref,
                                              std::string* error,
                                              bool* bad_message) OVERRIDE;
  virtual base::Value* BrowserToExtensionPref(
      const base::Value* browser_pref) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(ProxyPrefTransformer);
};

// This class observes proxy error events and routes them to the appropriate
// extensions listening to those events. All methods must be called on the IO
// thread unless otherwise specified.
class ProxyEventRouter {
 public:
  static ProxyEventRouter* GetInstance();

  void OnProxyError(EventRouterForwarder* event_router,
                    void* profile,
                    int error_code);

  void OnPACScriptError(EventRouterForwarder* event_router,
                        void* profile,
                        int line_number,
                        const string16& error);

 private:
  friend struct DefaultSingletonTraits<ProxyEventRouter>;

  ProxyEventRouter();
  ~ProxyEventRouter();

  DISALLOW_COPY_AND_ASSIGN(ProxyEventRouter);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_PROXY_PROXY_API_H_
