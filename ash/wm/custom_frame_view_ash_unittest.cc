// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/custom_frame_view_ash.h"

#include "ash/ash_switches.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/caption_buttons/frame_caption_button_container_view.h"
#include "ash/wm/caption_buttons/frame_maximize_button.h"
#include "ash/wm/caption_buttons/maximize_bubble_controller.h"
#include "ash/wm/property_util.h"
#include "ash/wm/window_util.h"
#include "ash/wm/workspace/snap_sizer.h"
#include "base/command_line.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/client/focus_client.h"
#include "ui/aura/test/event_generator.h"
#include "ui/aura/root_window.h"
#include "ui/aura/window.h"
#include "ui/base/events/event_utils.h"
#include "ui/base/gestures/gesture_configuration.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

namespace ash {
namespace test {

namespace {

class CancelCallbackHandler {
 public:
  CancelCallbackHandler(int update_events_before_cancel,
                        FrameMaximizeButton* maximize_button) :
      update_events_before_cancel_(update_events_before_cancel),
      maximize_button_(maximize_button) {}
  virtual ~CancelCallbackHandler() {}

  void CountedCancelCallback(ui::EventType event_type,
                             const gfx::Vector2dF& pos) {
    if (event_type == ui::ET_GESTURE_SCROLL_UPDATE &&
        !(--update_events_before_cancel_)) {
      // Make sure that we are in the middle of a resizing operation, cancel it
      // and then test that it is exited.
      EXPECT_TRUE(maximize_button_->is_snap_enabled());
      maximize_button_->DestroyMaximizeMenu();
      EXPECT_FALSE(maximize_button_->is_snap_enabled());
    }
  }

 private:
  // When this counter reaches 0, the gesture maximize action gets cancelled.
  int update_events_before_cancel_;

  // The maximize button which needs to get informed of the gesture termination.
  FrameMaximizeButton* maximize_button_;

  DISALLOW_COPY_AND_ASSIGN(CancelCallbackHandler);
};

class TestWidgetDelegate : public views::WidgetDelegateView {
 public:
  TestWidgetDelegate() {}
  virtual ~TestWidgetDelegate() {}

  // Overridden from views::WidgetDelegate:
  virtual views::View* GetContentsView() OVERRIDE {
    return this;
  }
  virtual bool CanResize() const OVERRIDE {
    return true;
  }
  virtual bool CanMaximize() const OVERRIDE {
    return true;
  }
  virtual views::NonClientFrameView* CreateNonClientFrameView(
      views::Widget* widget) OVERRIDE {
    // Always test CustomFrameViewAsh, which may not be the ash::Shell default.
    CustomFrameViewAsh* frame_view = new CustomFrameViewAsh;
    frame_view->Init(widget);
    return frame_view;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TestWidgetDelegate);
};

}  // namespace

class CustomFrameViewAshTest : public ash::test::AshTestBase {
 public:
  CustomFrameViewAshTest() {}
  virtual ~CustomFrameViewAshTest() {}

  views::Widget* CreateWidget() {
    views::Widget::InitParams params(views::Widget::InitParams::TYPE_WINDOW);
    views::Widget* widget = new views::Widget;
    params.context = CurrentContext();
    params.delegate = new TestWidgetDelegate;
    params.bounds = gfx::Rect(10, 10, 100, 100);
    params.opacity = views::Widget::InitParams::TRANSLUCENT_WINDOW;
    widget->Init(params);
    widget->Show();
    return widget;
  }

  void CloseWidget() {
    if (widget_)
      widget_->CloseNow();
    widget_ = NULL;
  }

  virtual void SetUp() OVERRIDE {
    AshTestBase::SetUp();

    CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kAshDisableAlternateFrameCaptionButtonStyle);

    widget_ = CreateWidget();
    CustomFrameViewAsh* frame = static_cast<CustomFrameViewAsh*>(
        widget()->non_client_view()->frame_view());
    FrameCaptionButtonContainerView::TestApi test(
        frame->caption_button_container_);
    maximize_button_ = static_cast<FrameMaximizeButton*>(
        test.size_button());
  }

  virtual void TearDown() OVERRIDE {
    CloseWidget();
    AshTestBase::TearDown();
  }

  views::Widget* widget() { return widget_; }

  FrameMaximizeButton* maximize_button() { return maximize_button_; }

 private:
  views::Widget* widget_;
  FrameMaximizeButton* maximize_button_;

  DISALLOW_COPY_AND_ASSIGN(CustomFrameViewAshTest);
};

// Tests that clicking on the resize-button toggles between maximize and normal
// state.
TEST_F(CustomFrameViewAshTest, ResizeButtonToggleMaximize) {
  aura::Window* window = widget()->GetNativeWindow();
  views::View* view = maximize_button();
  gfx::Point center = view->GetBoundsInScreen().CenterPoint();

  aura::test::EventGenerator generator(window->GetRootWindow(), center);

  EXPECT_FALSE(ash::wm::IsWindowMaximized(window));

  generator.ClickLeftButton();
  RunAllPendingInMessageLoop();
  EXPECT_TRUE(ash::wm::IsWindowMaximized(window));

  center = view->GetBoundsInScreen().CenterPoint();
  generator.MoveMouseTo(center);
  generator.ClickLeftButton();
  RunAllPendingInMessageLoop();
  EXPECT_FALSE(ash::wm::IsWindowMaximized(window));

  generator.GestureTapAt(view->GetBoundsInScreen().CenterPoint());
  EXPECT_TRUE(ash::wm::IsWindowMaximized(window));

  generator.GestureTapAt(view->GetBoundsInScreen().CenterPoint());
  EXPECT_FALSE(ash::wm::IsWindowMaximized(window));

  generator.GestureTapDownAndUp(view->GetBoundsInScreen().CenterPoint());
  EXPECT_TRUE(ash::wm::IsWindowMaximized(window));

  generator.GestureTapDownAndUp(view->GetBoundsInScreen().CenterPoint());
  EXPECT_FALSE(ash::wm::IsWindowMaximized(window));
}

#if defined(OS_WIN)
// RootWindow and Display can't resize on Windows Ash. http://crbug.com/165962
#define MAYBE_ResizeButtonDrag DISABLED_ResizeButtonDrag
#else
#define MAYBE_ResizeButtonDrag ResizeButtonDrag
#endif

// Tests that click+dragging on the resize-button tiles or minimizes the window.
TEST_F(CustomFrameViewAshTest, MAYBE_ResizeButtonDrag) {
  aura::Window* window = widget()->GetNativeWindow();
  views::View* view = maximize_button();
  gfx::Point center = view->GetBoundsInScreen().CenterPoint();

  aura::test::EventGenerator generator(window->GetRootWindow(), center);

  EXPECT_TRUE(ash::wm::IsWindowNormal(window));

  // Snap right.
  {
    generator.PressLeftButton();
    generator.MoveMouseBy(10, 0);
    generator.ReleaseLeftButton();
    RunAllPendingInMessageLoop();

    EXPECT_FALSE(ash::wm::IsWindowMaximized(window));
    EXPECT_FALSE(ash::wm::IsWindowMinimized(window));
    internal::SnapSizer sizer(window, center,
        internal::SnapSizer::RIGHT_EDGE,
        internal::SnapSizer::OTHER_INPUT);
    EXPECT_EQ(sizer.target_bounds().ToString(), window->bounds().ToString());
  }

  // Snap left.
  {
    center = view->GetBoundsInScreen().CenterPoint();
    generator.MoveMouseTo(center);
    generator.PressLeftButton();
    generator.MoveMouseBy(-10, 0);
    generator.ReleaseLeftButton();
    RunAllPendingInMessageLoop();

    EXPECT_FALSE(ash::wm::IsWindowMaximized(window));
    EXPECT_FALSE(ash::wm::IsWindowMinimized(window));
    internal::SnapSizer sizer(window, center,
        internal::SnapSizer::LEFT_EDGE,
        internal::SnapSizer::OTHER_INPUT);
    EXPECT_EQ(sizer.target_bounds().ToString(), window->bounds().ToString());
  }

  // Minimize.
  {
    center = view->GetBoundsInScreen().CenterPoint();
    generator.MoveMouseTo(center);
    generator.PressLeftButton();
    generator.MoveMouseBy(0, 10);
    generator.ReleaseLeftButton();
    RunAllPendingInMessageLoop();

    EXPECT_TRUE(ash::wm::IsWindowMinimized(window));
  }

  ash::wm::RestoreWindow(window);

  // Now test the same behaviour for gesture events.

  // Snap right.
  {
    center = view->GetBoundsInScreen().CenterPoint();
    gfx::Point end = center;
    end.Offset(80, 0);
    generator.GestureScrollSequence(center, end,
        base::TimeDelta::FromMilliseconds(100),
        3);
    RunAllPendingInMessageLoop();

    EXPECT_FALSE(ash::wm::IsWindowMaximized(window));
    EXPECT_FALSE(ash::wm::IsWindowMinimized(window));
    // This is a short resizing distance and different touch behavior
    // applies which leads in half of the screen being used.
    EXPECT_EQ("400,0 400x553", window->bounds().ToString());
  }

  // Snap left.
  {
    center = view->GetBoundsInScreen().CenterPoint();
    gfx::Point end = center;
    end.Offset(-80, 0);
    generator.GestureScrollSequence(center, end,
        base::TimeDelta::FromMilliseconds(100),
        3);
    RunAllPendingInMessageLoop();

    EXPECT_FALSE(ash::wm::IsWindowMaximized(window));
    EXPECT_FALSE(ash::wm::IsWindowMinimized(window));
    internal::SnapSizer sizer(window, center,
        internal::SnapSizer::LEFT_EDGE,
        internal::SnapSizer::OTHER_INPUT);
    EXPECT_EQ(sizer.target_bounds().ToString(), window->bounds().ToString());
  }

  // Minimize.
  {
    center = view->GetBoundsInScreen().CenterPoint();
    gfx::Point end = center;
    end.Offset(0, 40);
    generator.GestureScrollSequence(center, end,
        base::TimeDelta::FromMilliseconds(100),
        3);
    RunAllPendingInMessageLoop();

    EXPECT_TRUE(ash::wm::IsWindowMinimized(window));
  }

  // Test with gesture events.
}

#if defined(OS_WIN)
// RootWindow and Display can't resize on Windows Ash. http://crbug.com/165962
#define MAYBE_TouchDragResizeCloseToCornerDiffersFromMouse \
        DISABLED_TouchDragResizeCloseToCornerDiffersFromMouse
#else
#define MAYBE_TouchDragResizeCloseToCornerDiffersFromMouse \
        TouchDragResizeCloseToCornerDiffersFromMouse
#endif

// Tests Left/Right snapping with resize button touch dragging - which should
// trigger dependent on the available drag distance.
TEST_F(CustomFrameViewAshTest,
       MAYBE_TouchDragResizeCloseToCornerDiffersFromMouse) {
  aura::Window* window = widget()->GetNativeWindow();
  views::View* view = maximize_button();

  gfx::Rect work_area = widget()->GetWorkAreaBoundsInScreen();
  gfx::Rect bounds = window->bounds();
  bounds.set_x(work_area.width() - bounds.width());
  widget()->SetBounds(bounds);

  gfx::Point start_point = view->GetBoundsInScreen().CenterPoint();
  // We want to move all the way to the right (the few pixels we have).
  gfx::Point end_point = gfx::Point(work_area.width(), start_point.y());

  aura::test::EventGenerator generator(window->GetRootWindow(), start_point);

  EXPECT_TRUE(ash::wm::IsWindowNormal(window));

  // Snap right with a touch drag.
  generator.GestureScrollSequence(start_point,
                                  end_point,
                                  base::TimeDelta::FromMilliseconds(100),
                                  10);
  RunAllPendingInMessageLoop();

  EXPECT_FALSE(ash::wm::IsWindowMaximized(window));
  EXPECT_FALSE(ash::wm::IsWindowMinimized(window));
  gfx::Rect touch_result = window->bounds();
  EXPECT_NE(bounds.ToString(), touch_result.ToString());

  // Set the position back to where it was before and re-try with a mouse.
  widget()->SetBounds(bounds);

  generator.MoveMouseTo(start_point);
  generator.PressLeftButton();
  generator.MoveMouseTo(end_point, 10);
  generator.ReleaseLeftButton();
  RunAllPendingInMessageLoop();

  EXPECT_FALSE(ash::wm::IsWindowMaximized(window));
  EXPECT_FALSE(ash::wm::IsWindowMinimized(window));
  gfx::Rect mouse_result = window->bounds();

  // The difference between the two operations should be that the mouse
  // operation should have just started to resize and the touch operation is
  // already all the way down to the smallest possible size.
  EXPECT_NE(mouse_result.ToString(), touch_result.ToString());
  EXPECT_GT(mouse_result.width(), touch_result.width());
}

// Test that closing the (browser) window with an opened balloon does not
// crash the system. In other words: Make sure that shutting down the frame
// destroys the opened balloon in an orderly fashion.
TEST_F(CustomFrameViewAshTest, MaximizeButtonExternalShutDown) {
  aura::Window* window = widget()->GetNativeWindow();
  ash::FrameMaximizeButton* maximize_button =
      CustomFrameViewAshTest::maximize_button();
  maximize_button->set_bubble_appearance_delay_ms(0);
  gfx::Point button_pos = maximize_button->GetBoundsInScreen().CenterPoint();
  gfx::Point off_pos(button_pos.x() + 100, button_pos.y() + 100);

  aura::test::EventGenerator generator(window->GetRootWindow(), off_pos);
  EXPECT_FALSE(maximize_button->maximizer());
  EXPECT_TRUE(ash::wm::IsWindowNormal(window));

  // Move the mouse cursor over the button to bring up the maximizer bubble.
  generator.MoveMouseTo(button_pos);
  EXPECT_TRUE(maximize_button->maximizer());

  // Even though the widget is closing the bubble menu should not crash upon
  // its delayed destruction.
  CloseWidget();
}

// Test that maximizing the browser after hovering in does not crash the system
// when the observer gets removed in the bubble destruction process.
TEST_F(CustomFrameViewAshTest, MaximizeOnHoverThenClick) {
  aura::Window* window = widget()->GetNativeWindow();
  ash::FrameMaximizeButton* maximize_button =
      CustomFrameViewAshTest::maximize_button();
  maximize_button->set_bubble_appearance_delay_ms(0);
  gfx::Point button_pos = maximize_button->GetBoundsInScreen().CenterPoint();
  gfx::Point off_pos(button_pos.x() + 100, button_pos.y() + 100);

  aura::test::EventGenerator generator(window->GetRootWindow(), off_pos);
  EXPECT_FALSE(maximize_button->maximizer());
  EXPECT_TRUE(ash::wm::IsWindowNormal(window));

  // Move the mouse cursor over the button to bring up the maximizer bubble.
  generator.MoveMouseTo(button_pos);
  EXPECT_TRUE(maximize_button->maximizer());
  generator.ClickLeftButton();
  EXPECT_TRUE(ash::wm::IsWindowMaximized(window));
}

// Test that hovering over a button in the balloon dialog will show the phantom
// window. Moving then away from the button will hide it again. Then check that
// pressing and dragging the button itself off the button will also release the
// phantom window.
TEST_F(CustomFrameViewAshTest, MaximizeLeftButtonDragOut) {
  aura::Window* window = widget()->GetNativeWindow();
  ash::FrameMaximizeButton* maximize_button =
      CustomFrameViewAshTest::maximize_button();
  maximize_button->set_bubble_appearance_delay_ms(0);
  gfx::Point button_pos = maximize_button->GetBoundsInScreen().CenterPoint();
  gfx::Point off_pos(button_pos.x() + 100, button_pos.y() + 100);

  aura::test::EventGenerator generator(window->GetRootWindow(), off_pos);
  EXPECT_FALSE(maximize_button->maximizer());
  EXPECT_TRUE(ash::wm::IsWindowNormal(window));
  EXPECT_FALSE(maximize_button->phantom_window_open());

  // Move the mouse cursor over the button to bring up the maximizer bubble.
  generator.MoveMouseTo(button_pos);
  EXPECT_TRUE(maximize_button->maximizer());

  // Move the mouse over the left maximize button.
  gfx::Point left_max_pos = maximize_button->maximizer()->
      GetButtonForUnitTest(SNAP_LEFT)->GetBoundsInScreen().CenterPoint();

  generator.MoveMouseTo(left_max_pos);
  // Expect the phantom window to be open.
  EXPECT_TRUE(maximize_button->phantom_window_open());

  // Move away to see the window being destroyed.
  generator.MoveMouseTo(off_pos);
  EXPECT_FALSE(maximize_button->phantom_window_open());

  // Move back over the button.
  generator.MoveMouseTo(button_pos);
  generator.MoveMouseTo(left_max_pos);
  EXPECT_TRUE(maximize_button->phantom_window_open());

  // Press button and drag out of dialog.
  generator.PressLeftButton();
  generator.MoveMouseTo(off_pos);
  generator.ReleaseLeftButton();

  // Check that the phantom window is also gone.
  EXPECT_FALSE(maximize_button->phantom_window_open());
}

// Test that clicking a button in the maximizer bubble (in this case the
// maximize left button) will do the requested action.
TEST_F(CustomFrameViewAshTest, MaximizeLeftByButton) {
  aura::Window* window = widget()->GetNativeWindow();
  ash::FrameMaximizeButton* maximize_button =
      CustomFrameViewAshTest::maximize_button();
  maximize_button->set_bubble_appearance_delay_ms(0);
  gfx::Point button_pos = maximize_button->GetBoundsInScreen().CenterPoint();
  gfx::Point off_pos(button_pos.x() + 100, button_pos.y() + 100);

  aura::test::EventGenerator generator(window->GetRootWindow(), off_pos);
  EXPECT_FALSE(maximize_button->maximizer());
  EXPECT_TRUE(ash::wm::IsWindowNormal(window));
  EXPECT_FALSE(maximize_button->phantom_window_open());

  // Move the mouse cursor over the button to bring up the maximizer bubble.
  generator.MoveMouseTo(button_pos);
  EXPECT_TRUE(maximize_button->maximizer());

  // Move the mouse over the left maximize button.
  gfx::Point left_max_pos = maximize_button->maximizer()->
      GetButtonForUnitTest(SNAP_LEFT)->GetBoundsInScreen().CenterPoint();
  generator.MoveMouseTo(left_max_pos);
  EXPECT_TRUE(maximize_button->phantom_window_open());
  generator.ClickLeftButton();

  EXPECT_FALSE(maximize_button->maximizer());
  EXPECT_FALSE(maximize_button->phantom_window_open());

  EXPECT_FALSE(ash::wm::IsWindowMaximized(window));
  EXPECT_FALSE(ash::wm::IsWindowMinimized(window));
  internal::SnapSizer sizer(window, button_pos,
                            internal::SnapSizer::LEFT_EDGE,
                            internal::SnapSizer::OTHER_INPUT);
  sizer.SelectDefaultSizeAndDisableResize();
  EXPECT_EQ(sizer.target_bounds().ToString(), window->bounds().ToString());
}

// Test that the activation focus does not change when the bubble gets shown.
TEST_F(CustomFrameViewAshTest, MaximizeKeepFocus) {
  aura::Window* window = widget()->GetNativeWindow();
  ash::FrameMaximizeButton* maximize_button =
      CustomFrameViewAshTest::maximize_button();
  maximize_button->set_bubble_appearance_delay_ms(0);
  gfx::Point button_pos = maximize_button->GetBoundsInScreen().CenterPoint();
  gfx::Point off_pos(button_pos.x() + 100, button_pos.y() + 100);

  aura::test::EventGenerator generator(window->GetRootWindow(), off_pos);
  EXPECT_FALSE(maximize_button->maximizer());
  EXPECT_TRUE(ash::wm::IsWindowNormal(window));

  aura::Window* active =
      aura::client::GetFocusClient(window)->GetFocusedWindow();

  // Move the mouse cursor over the button to bring up the maximizer bubble.
  generator.MoveMouseTo(button_pos);
  EXPECT_TRUE(maximize_button->maximizer());

  // Check that the focused window is still the same.
  EXPECT_EQ(active, aura::client::GetFocusClient(window)->GetFocusedWindow());
}

TEST_F(CustomFrameViewAshTest, MaximizeTap) {
  aura::Window* window = widget()->GetNativeWindow();
  aura::RootWindow* root_window = window->GetRootWindow();
  ash::FrameMaximizeButton* maximize_button =
      CustomFrameViewAshTest::maximize_button();
  gfx::Point button_pos = maximize_button->GetBoundsInScreen().CenterPoint();

  const int touch_default_radius =
      ui::GestureConfiguration::default_radius();
  ui::GestureConfiguration::set_default_radius(0);

  const int kTouchId = 2;
  ui::TouchEvent press(ui::ET_TOUCH_PRESSED,
                       button_pos,
                       kTouchId,
                       ui::EventTimeForNow());
  root_window->AsRootWindowHostDelegate()->OnHostTouchEvent(&press);

  button_pos.Offset(9, 8);
  ui::TouchEvent release(
      ui::ET_TOUCH_RELEASED,
      button_pos,
      kTouchId,
      press.time_stamp() + base::TimeDelta::FromMilliseconds(50));
  root_window->AsRootWindowHostDelegate()->OnHostTouchEvent(&release);

  ui::GestureConfiguration::set_default_radius(touch_default_radius);
}

// Test that only the left button will activate the maximize button.
TEST_F(CustomFrameViewAshTest, OnlyLeftButtonMaximizes) {
  aura::Window* window = widget()->GetNativeWindow();
  ash::FrameMaximizeButton* maximize_button =
      CustomFrameViewAshTest::maximize_button();
  maximize_button->set_bubble_appearance_delay_ms(0);
  gfx::Point button_pos = maximize_button->GetBoundsInScreen().CenterPoint();
  gfx::Point off_pos(button_pos.x() + 100, button_pos.y() + 100);

  aura::test::EventGenerator generator(window->GetRootWindow(), off_pos);
  EXPECT_FALSE(maximize_button->maximizer());
  EXPECT_TRUE(ash::wm::IsWindowNormal(window));
  EXPECT_FALSE(ash::wm::IsWindowMaximized(window));

  // Move the mouse cursor over the button.
  generator.MoveMouseTo(button_pos);
  EXPECT_TRUE(maximize_button->maximizer());
  EXPECT_FALSE(maximize_button->phantom_window_open());

  // After pressing the left button the button should get triggered.
  generator.PressLeftButton();
  RunAllPendingInMessageLoop();
  EXPECT_TRUE(maximize_button->is_snap_enabled());
  EXPECT_FALSE(ash::wm::IsWindowMaximized(window));

  // Pressing the right button then should cancel the operation.
  generator.PressRightButton();
  RunAllPendingInMessageLoop();
  EXPECT_FALSE(maximize_button->maximizer());

  // After releasing the second button the window shouldn't be maximized.
  generator.ReleaseRightButton();
  generator.ReleaseLeftButton();
  RunAllPendingInMessageLoop();
  EXPECT_FALSE(ash::wm::IsWindowMaximized(window));

  // Second experiment: Starting with right should also not trigger.
  generator.MoveMouseTo(off_pos);
  generator.MoveMouseTo(button_pos);
  EXPECT_TRUE(maximize_button->maximizer());

  // Pressing first the right button should not activate.
  generator.PressRightButton();
  RunAllPendingInMessageLoop();
  EXPECT_FALSE(maximize_button->is_snap_enabled());

  // Pressing then additionally the left button shouldn't activate either.
  generator.PressLeftButton();
  RunAllPendingInMessageLoop();
  EXPECT_FALSE(maximize_button->is_snap_enabled());
  generator.ReleaseRightButton();
  generator.ReleaseLeftButton();
  EXPECT_FALSE(ash::wm::IsWindowMaximized(window));
}

// Click a button of window maximize functionality.
// If |snap_type| is SNAP_NONE the FrameMaximizeButton gets clicked, otherwise
// the associated snap button.
// |Window| is the window which owns the maximize button.
// |maximize_button| is the FrameMaximizeButton which controls the window.
void ClickMaxButton(
    ash::FrameMaximizeButton* maximize_button,
    aura::Window* window,
    SnapType snap_type) {
  gfx::Point button_pos = maximize_button->GetBoundsInScreen().CenterPoint();
  gfx::Point off_pos(button_pos.x() + 100, button_pos.y() + 100);

  aura::test::EventGenerator generator(window->GetRootWindow(), off_pos);
  generator.MoveMouseTo(off_pos);
  EXPECT_FALSE(maximize_button->maximizer());
  EXPECT_FALSE(maximize_button->phantom_window_open());

  // Move the mouse cursor over the button.
  generator.MoveMouseTo(button_pos);
  EXPECT_TRUE(maximize_button->maximizer());
  EXPECT_FALSE(maximize_button->phantom_window_open());

  if (snap_type != SNAP_NONE) {
    gfx::Point left_max_pos = maximize_button->maximizer()->
        GetButtonForUnitTest(snap_type)->GetBoundsInScreen().CenterPoint();
    generator.MoveMouseTo(left_max_pos);
    EXPECT_TRUE(maximize_button->phantom_window_open());
  }
  // After pressing the left button the button should get triggered.
  generator.ClickLeftButton();
  EXPECT_FALSE(maximize_button->maximizer());
}

// Test that the restore from left/right maximize is properly done.
TEST_F(CustomFrameViewAshTest, MaximizeLeftRestore) {
  aura::Window* window = widget()->GetNativeWindow();
  gfx::Rect initial_bounds = widget()->GetWindowBoundsInScreen();
  ash::FrameMaximizeButton* maximize_button =
      CustomFrameViewAshTest::maximize_button();
  maximize_button->set_bubble_appearance_delay_ms(0);

  ClickMaxButton(maximize_button, window, SNAP_LEFT);
  // The window should not be maximized.
  EXPECT_FALSE(ash::wm::IsWindowMaximized(window));
  // But the bounds should be different.
  gfx::Rect new_bounds = widget()->GetWindowBoundsInScreen();
  EXPECT_EQ(0, new_bounds.x());
  EXPECT_EQ(0, new_bounds.y());

  // Now click the same button again to see that it restores.
  ClickMaxButton(maximize_button, window, SNAP_LEFT);
  // But the bounds should be restored.
  new_bounds = widget()->GetWindowBoundsInScreen();
  EXPECT_EQ(new_bounds.x(), initial_bounds.x());
  EXPECT_EQ(new_bounds.y(), initial_bounds.x());
  EXPECT_EQ(new_bounds.width(), initial_bounds.width());
  EXPECT_EQ(new_bounds.height(), initial_bounds.height());
  // Make sure that there is no restore rectangle left.
  EXPECT_EQ(NULL, GetRestoreBoundsInScreen(window));
}

// Maximize, left/right maximize and then restore should works.
TEST_F(CustomFrameViewAshTest, MaximizeMaximizeLeftRestore) {
  aura::Window* window = widget()->GetNativeWindow();
  gfx::Rect initial_bounds = widget()->GetWindowBoundsInScreen();
  ash::FrameMaximizeButton* maximize_button =
      CustomFrameViewAshTest::maximize_button();
  maximize_button->set_bubble_appearance_delay_ms(0);

  ClickMaxButton(maximize_button, window, SNAP_NONE);
  EXPECT_TRUE(ash::wm::IsWindowMaximized(window));

  ClickMaxButton(maximize_button, window, SNAP_LEFT);
  EXPECT_FALSE(ash::wm::IsWindowMaximized(window));
  gfx::Rect new_bounds = widget()->GetWindowBoundsInScreen();
  EXPECT_EQ(0, new_bounds.x());
  EXPECT_EQ(0, new_bounds.y());

  // Now click the same button again to see that it restores.
  ClickMaxButton(maximize_button, window, SNAP_LEFT);
  RunAllPendingInMessageLoop();
  // But the bounds should be restored.
  new_bounds = widget()->GetWindowBoundsInScreen();
  EXPECT_EQ(new_bounds.x(), initial_bounds.x());
  EXPECT_EQ(new_bounds.y(), initial_bounds.x());
  EXPECT_EQ(new_bounds.width(), initial_bounds.width());
  EXPECT_EQ(new_bounds.height(), initial_bounds.height());
  // Make sure that there is no restore rectangle left.
  EXPECT_EQ(NULL, GetRestoreBoundsInScreen(window));
}

// Left/right maximize, maximize and then restore should work.
TEST_F(CustomFrameViewAshTest, MaximizeLeftMaximizeRestore) {
  aura::Window* window = widget()->GetNativeWindow();
  gfx::Rect initial_bounds = widget()->GetWindowBoundsInScreen();
  ash::FrameMaximizeButton* maximize_button =
      CustomFrameViewAshTest::maximize_button();
  maximize_button->set_bubble_appearance_delay_ms(0);

  ClickMaxButton(maximize_button, window, SNAP_LEFT);
  EXPECT_FALSE(ash::wm::IsWindowMaximized(window));

  ClickMaxButton(maximize_button, window, SNAP_NONE);
  EXPECT_TRUE(ash::wm::IsWindowMaximized(window));

  ClickMaxButton(maximize_button, window, SNAP_NONE);
  EXPECT_FALSE(ash::wm::IsWindowMaximized(window));
  gfx::Rect new_bounds = widget()->GetWindowBoundsInScreen();
  EXPECT_EQ(new_bounds.x(), initial_bounds.x());
  EXPECT_EQ(new_bounds.y(), initial_bounds.x());
  EXPECT_EQ(new_bounds.width(), initial_bounds.width());
  EXPECT_EQ(new_bounds.height(), initial_bounds.height());
  // Make sure that there is no restore rectangle left.
  EXPECT_EQ(NULL, GetRestoreBoundsInScreen(window));
}

// Starting with a window which has no restore bounds, maximize then left/right
// maximize should not be centered but left/right maximized.
TEST_F(CustomFrameViewAshTest, MaximizeThenLeftMaximize) {
  aura::Window* window = widget()->GetNativeWindow();
  gfx::Rect initial_bounds = widget()->GetWindowBoundsInScreen();
  ash::FrameMaximizeButton* maximize_button =
      CustomFrameViewAshTest::maximize_button();
  maximize_button->set_bubble_appearance_delay_ms(0);
  // Make sure that there is no restore rectangle.
  EXPECT_EQ(NULL, GetRestoreBoundsInScreen(window));

  ClickMaxButton(maximize_button, window, SNAP_NONE);
  EXPECT_TRUE(ash::wm::IsWindowMaximized(window));

  ClickMaxButton(maximize_button, window, SNAP_LEFT);
  EXPECT_FALSE(ash::wm::IsWindowMaximized(window));

  gfx::Rect new_bounds = widget()->GetWindowBoundsInScreen();
  EXPECT_EQ(new_bounds.x(), 0);
  EXPECT_EQ(new_bounds.y(), 0);
  // Make sure that the restore rectangle is the original rectangle.
  EXPECT_EQ(initial_bounds.ToString(),
            GetRestoreBoundsInScreen(window)->ToString());
}

// Test that minimizing the window per keyboard closes the maximize bubble.
TEST_F(CustomFrameViewAshTest, MinimizePerKeyClosesBubble) {
  aura::Window* window = widget()->GetNativeWindow();
  ash::FrameMaximizeButton* maximize_button =
      CustomFrameViewAshTest::maximize_button();

  gfx::Point button_pos = maximize_button->GetBoundsInScreen().CenterPoint();
  gfx::Point off_pos(button_pos.x() + 100, button_pos.y() + 100);

  aura::test::EventGenerator generator(window->GetRootWindow(), off_pos);
  generator.MoveMouseTo(off_pos);
  EXPECT_FALSE(maximize_button->maximizer());

  // Move the mouse cursor over the maximize button.
  generator.MoveMouseTo(button_pos);
  EXPECT_TRUE(maximize_button->maximizer());

  // We simulate the keystroke by calling minimizeWindow directly.
  wm::MinimizeWindow(window);

  EXPECT_TRUE(ash::wm::IsWindowMinimized(window));
  EXPECT_FALSE(maximize_button->maximizer());
}

// Tests that dragging down on the maximize button minimizes the window.
TEST_F(CustomFrameViewAshTest, MaximizeButtonDragDownMinimizes) {
  aura::Window* window = widget()->GetNativeWindow();
  ash::FrameMaximizeButton* maximize_button =
      CustomFrameViewAshTest::maximize_button();

  // Drag down on a maximized window.
  wm::MaximizeWindow(window);
  EXPECT_TRUE(wm::IsWindowMaximized(window));
  gfx::Point button_pos = maximize_button->GetBoundsInScreen().CenterPoint();
  gfx::Point off_pos(button_pos.x(), button_pos.y() + 100);

  aura::test::EventGenerator generator(window->GetRootWindow());
  generator.GestureScrollSequence(button_pos, off_pos,
      base::TimeDelta::FromMilliseconds(0), 1);

  EXPECT_TRUE(wm::IsWindowMinimized(window));
  EXPECT_FALSE(maximize_button->maximizer());

  // Drag down on a restored window.
  wm::RestoreWindow(window);

  button_pos = maximize_button->GetBoundsInScreen().CenterPoint();
  off_pos = gfx::Point(button_pos.x(), button_pos.y() + 200);
  generator.GestureScrollSequence(button_pos, off_pos,
      base::TimeDelta::FromMilliseconds(10), 1);
  EXPECT_TRUE(wm::IsWindowMinimized(window));
  EXPECT_FALSE(maximize_button->maximizer());
}

// Tests that dragging Left and pressing ESC does properly abort.
TEST_F(CustomFrameViewAshTest, MaximizeButtonDragLeftEscapeExits) {
  aura::Window* window = widget()->GetNativeWindow();
  gfx::Rect initial_bounds = widget()->GetWindowBoundsInScreen();
  ash::FrameMaximizeButton* maximize_button =
      CustomFrameViewAshTest::maximize_button();

  gfx::Point button_pos = maximize_button->GetBoundsInScreen().CenterPoint();
  gfx::Point off_pos(button_pos.x() - button_pos.x() / 2, button_pos.y());

  const int kGestureSteps = 10;
  CancelCallbackHandler cancel_handler(kGestureSteps / 2, maximize_button);
  aura::test::EventGenerator generator(window->GetRootWindow());
  generator.GestureScrollSequenceWithCallback(
      button_pos,
      off_pos,
      base::TimeDelta::FromMilliseconds(0),
      kGestureSteps,
      base::Bind(&CancelCallbackHandler::CountedCancelCallback,
                 base::Unretained(&cancel_handler)));

  // Check that there was no size change.
  EXPECT_EQ(widget()->GetWindowBoundsInScreen().size().ToString(),
            initial_bounds.size().ToString());
  // Check that there is no phantom window left open.
  EXPECT_FALSE(maximize_button->phantom_window_open());
}

}  // namespace test
}  // namespace ash
