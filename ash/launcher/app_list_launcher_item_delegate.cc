// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/launcher/app_list_launcher_item_delegate.h"

#include "ash/launcher/launcher_item_delegate_manager.h"
#include "ash/launcher/launcher_model.h"
#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "grit/ash_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash {
namespace internal {

AppListLauncherItemDelegate::AppListLauncherItemDelegate() {
  LauncherItem app_list;
  app_list.type = TYPE_APP_LIST;
  Shell::GetInstance()->launcher_model()->Add(app_list);

  ash::Shell::GetInstance()->launcher_item_delegate_manager()->
      RegisterLauncherItemDelegate(ash::TYPE_APP_LIST, this);
}

AppListLauncherItemDelegate::~AppListLauncherItemDelegate() {
  // Don't unregister this from LauncherItemDelegateManager.
  // LauncherItemDelegateManager is already destroyed.
}

void AppListLauncherItemDelegate::ItemSelected(const LauncherItem& item,
                                               const ui::Event& event) {
  // Pass NULL here to show the app list in the currently active RootWindow.
  Shell::GetInstance()->ToggleAppList(NULL);
}

base::string16 AppListLauncherItemDelegate::GetTitle(const LauncherItem& item) {
  LauncherModel* model = Shell::GetInstance()->launcher_model();
  DCHECK(model);
  return model->status() == LauncherModel::STATUS_LOADING ?
      l10n_util::GetStringUTF16(IDS_AURA_APP_LIST_SYNCING_TITLE) :
      l10n_util::GetStringUTF16(IDS_AURA_APP_LIST_TITLE);
}

ui::MenuModel* AppListLauncherItemDelegate::CreateContextMenu(
    const LauncherItem& item,
    aura::RootWindow* root_window) {
  return NULL;
}

LauncherMenuModel* AppListLauncherItemDelegate::CreateApplicationMenu(
    const LauncherItem& item,
    int event_flags) {
  // AppList does not show an application menu.
  return NULL;
}

bool AppListLauncherItemDelegate::IsDraggable(const LauncherItem& item) {
  return false;
}

bool AppListLauncherItemDelegate::ShouldShowTooltip(const LauncherItem& item) {
  return true;
}

}  // namespace internal
}  // namespace ash
