// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_INTENTS_DISPATCHER_H_
#define CONTENT_RENDERER_INTENTS_DISPATCHER_H_
#pragma once

#include "base/memory/scoped_ptr.h"
#include "content/renderer/render_view_observer.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebString.h"

namespace webkit_glue {
struct WebIntentData;
}

namespace WebKit {
class WebFrame;
}

// IntentsDispatcher is a delegate for Web Intents messages. It is the
// renderer-side handler for IPC messages delivering the intent payload data
// and preparing it for access by the service page.
class IntentsDispatcher : public RenderViewObserver {
 public:
  // |render_view| must not be NULL.
  explicit IntentsDispatcher(RenderView* render_view);
  virtual ~IntentsDispatcher();

  // Called by the bound intent object to register the result from the service
  // page.
  void OnResult(const WebKit::WebString& data);
  void OnFailure(const WebKit::WebString& data);

 private:
  class BoundDeliveredIntent;

  // RenderView::Observer implementation.
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;
  virtual void DidClearWindowObject(WebKit::WebFrame* frame) OVERRIDE;

  // TODO(gbillock): Do we need various ***ClientRedirect methods to implement
  // intent cancelling policy? Figure this out.

  // Handler method for the IntentsMsg_SetWebIntent message.
  void OnSetIntent(const webkit_glue::WebIntentData& intent, int intent_id);

  // Delivered intent data from the caller.
  scoped_ptr<webkit_glue::WebIntentData> intent_;
  // Delivered intent id from the caller.
  int intent_id_;

  // Representation of the intent data as a C++ bound NPAPI object to be
  // injected into the Javascript context.
  scoped_ptr<BoundDeliveredIntent> delivered_intent_;

  DISALLOW_COPY_AND_ASSIGN(IntentsDispatcher);
};

#endif  // CONTENT_RENDERER_INTENTS_DISPATCHER_H_
