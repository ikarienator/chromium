// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_WINDOW_PROPERTIES_H_
#define ASH_WM_WINDOW_PROPERTIES_H_

#include "ash/ash_export.h"
#include "ash/wm/property_util.h"
#include "ui/aura/window.h"
#include "ui/base/ui_base_types.h"

namespace gfx {
class Rect;
}

namespace ash {
namespace internal {

// Shell-specific window property keys.

// Alphabetical sort.

//  A property key to suppress the cross-fade animation for the transition to
// the fullscreen state.
extern const aura::WindowProperty<bool>* const kAnimateToFullscreenKey;

// A property key to indicate whether there is any chrome at all that cannot be
// hidden when the window is fullscreen. This is unrelated to whether the full
// chrome can be revealed by hovering the mouse at the top of the screen.
ASH_EXPORT extern const aura::WindowProperty<bool>* const
    kFullscreenUsesMinimalChromeKey;

// If this is set to true, the window stays in the same root window
// even if the bounds outside of its root window is set.
// This is exported as it's used in the tests.
ASH_EXPORT extern const aura::WindowProperty<bool>* const
    kStayInSameRootWindowKey;

// A property to remember the window position which was set before the
// auto window position manager changed the window bounds, so that it can get
// restored when only this one window gets shown.
ASH_EXPORT extern const aura::WindowProperty<gfx::Rect*>* const
    kPreAutoManagedWindowBoundsKey;

// Property to tell if the container uses the screen coordinates.
extern const aura::WindowProperty<bool>* const kUsesScreenCoordinatesKey;

// A property key to tell the workspace layout manager to always restore the
// window to the restore-bounds (false by default).
extern const aura::WindowProperty<bool>* const kWindowRestoresToRestoreBounds;

// Alphabetical sort.

}  // namespace internal
}  // namespace ash

#endif  // ASH_WM_WINDOW_PROPERTIES_H_
