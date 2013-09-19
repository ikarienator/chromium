// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/input/input.h"

#include "ash/root_window_controller.h"
#include "base/lazy_instance.h"
#include "base/strings/string16.h"
#include "chrome/browser/extensions/extension_function_registry.h"
#include "content/public/browser/browser_thread.h"
#include "ui/events/event.h"
#include "ui/keyboard/keyboard_controller.h"

#if defined(USE_ASH)
#include "ash/shell.h"
#include "ui/keyboard/keyboard_util.h"
#endif

namespace {

const char kNotYetImplementedError[] =
    "API is not implemented on this platform.";

}  // namespace

namespace extensions {

bool VirtualKeyboardPrivateInsertTextFunction::RunImpl() {
#if defined(USE_ASH)
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  string16 text;
  EXTENSION_FUNCTION_VALIDATE(args_->GetString(0, &text));

  return keyboard::InsertText(text, ash::Shell::GetPrimaryRootWindow());
#endif
  error_ = kNotYetImplementedError;
  return false;
}

bool VirtualKeyboardPrivateMoveCursorFunction::RunImpl() {
#if defined(USE_ASH)
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  int swipe_direction;
  int modifier_flags;
  EXTENSION_FUNCTION_VALIDATE(args_->GetInteger(0, &swipe_direction));
  EXTENSION_FUNCTION_VALIDATE(args_->GetInteger(1, &modifier_flags));

  return keyboard::MoveCursor(swipe_direction, modifier_flags,
                              ash::Shell::GetPrimaryRootWindow());
#endif
  error_ = kNotYetImplementedError;
  return false;
}

bool VirtualKeyboardPrivateSendKeyEventFunction::RunImpl() {
#if defined(USE_ASH)
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  base::Value* options_value = NULL;
  base::DictionaryValue* params = NULL;
  std::string type;
  int char_value;
  int key_code;
  bool shift_modifier;

  EXTENSION_FUNCTION_VALIDATE(args_->Get(0, &options_value));
  EXTENSION_FUNCTION_VALIDATE(options_value->GetAsDictionary(&params));
  EXTENSION_FUNCTION_VALIDATE(params->GetString("type", &type));
  EXTENSION_FUNCTION_VALIDATE(params->GetInteger("charValue", &char_value));
  EXTENSION_FUNCTION_VALIDATE(params->GetInteger("keyCode", &key_code));
  EXTENSION_FUNCTION_VALIDATE(params->GetBoolean("shiftKey", &shift_modifier));

  return keyboard::SendKeyEvent(type,
                                char_value,
                                key_code,
                                shift_modifier,
                                ash::Shell::GetPrimaryRootWindow());
#endif
  error_ = kNotYetImplementedError;
  return false;
}

bool VirtualKeyboardPrivateHideKeyboardFunction::RunImpl() {
#if defined(USE_ASH)
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  ash::Shell::GetPrimaryRootWindowController()->keyboard_controller()->
      HideKeyboard();

  return true;
#endif
  error_ = kNotYetImplementedError;
  return false;
}

InputAPI::InputAPI(Profile* profile) {
}

InputAPI::~InputAPI() {
}

static base::LazyInstance<ProfileKeyedAPIFactory<InputAPI> >
g_factory = LAZY_INSTANCE_INITIALIZER;

// static
ProfileKeyedAPIFactory<InputAPI>* InputAPI::GetFactoryInstance() {
  return &g_factory.Get();
}

}  // namespace extensions
