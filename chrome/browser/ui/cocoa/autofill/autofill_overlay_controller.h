// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_AUTOFILL_AUTOFILL_OVERLAY_CONTROLLER_H_
#define CHROME_BROWSER_UI_COCOA_AUTOFILL_AUTOFILL_OVERLAY_CONTROLLER_H_

#import <Cocoa/Cocoa.h>

#include "base/mac/scoped_nsobject.h"
#include "base/memory/scoped_ptr.h"
#import "chrome/browser/ui/cocoa/autofill/autofill_layout.h"

namespace autofill {
class AutofillDialogViewDelegate;
struct DialogOverlayState;
}  // autofill

@class AutofillMessageStackView;

@interface AutofillOverlayController :
    NSViewController<AutofillLayout> {
 @private
  // |childView_| contains all overlay UI elements. This is used to fade out
  // UI elements first, before making the main view transparent to fade out the
  // overlay shield.
  base::scoped_nsobject<NSView> childView_;
  base::scoped_nsobject<NSImageView> imageView_;
  base::scoped_nsobject<AutofillMessageStackView> messageStackView_;

  autofill::AutofillDialogViewDelegate* delegate_;  // not owned, owns dialog.
}

// Designated initializer.
- (id)initWithDelegate:(autofill::AutofillDialogViewDelegate*)delegate;

// Updates the state from the dialog controller.
- (void)updateState;

// Get the preferred view height for a given width.
- (CGFloat)heightForWidth:(int)width;

@end

#endif  // CHROME_BROWSER_UI_COCOA_AUTOFILL_AUTOFILL_OVERLAY_CONTROLLER_H_

