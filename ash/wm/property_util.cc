// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/property_util.h"

#include "ash/screen_ash.h"
#include "ash/wm/window_properties.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/base/ui_base_types.h"
#include "ui/gfx/rect.h"

namespace ash {

void SetRestoreBoundsInScreen(aura::Window* window, const gfx::Rect& bounds) {
  window->SetProperty(aura::client::kRestoreBoundsKey, new gfx::Rect(bounds));
}

void SetRestoreBoundsInParent(aura::Window* window, const gfx::Rect& bounds) {
  SetRestoreBoundsInScreen(window,
      ScreenAsh::ConvertRectToScreen(window->parent(), bounds));
}

const gfx::Rect* GetRestoreBoundsInScreen(aura::Window* window) {
  return window->GetProperty(aura::client::kRestoreBoundsKey);
}

gfx::Rect GetRestoreBoundsInParent(aura::Window* window) {
  const gfx::Rect* rect = GetRestoreBoundsInScreen(window);
  if (!rect)
    return gfx::Rect();
  return ScreenAsh::ConvertRectFromScreen(window->parent(), *rect);
}

void ClearRestoreBounds(aura::Window* window) {
  window->ClearProperty(aura::client::kRestoreBoundsKey);
}

void SetWindowAlwaysRestoresToRestoreBounds(aura::Window* window, bool value) {
  window->SetProperty(internal::kWindowRestoresToRestoreBounds, value);
}

bool GetWindowAlwaysRestoresToRestoreBounds(const aura::Window* window) {
  return window->GetProperty(internal::kWindowRestoresToRestoreBounds);
}

}  // namespace ash
