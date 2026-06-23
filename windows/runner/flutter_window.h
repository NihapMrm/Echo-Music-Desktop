#ifndef RUNNER_FLUTTER_WINDOW_H_
#define RUNNER_FLUTTER_WINDOW_H_

#include <flutter/dart_project.h>
#include <flutter/flutter_view_controller.h>
#include <flutter/method_channel.h>
#include <flutter/standard_method_codec.h>
#include <shobjidl.h>

#include <memory>

#include "win32_window.h"

// A window that does nothing but host a Flutter view.
class FlutterWindow : public Win32Window {
 public:
  // Creates a new FlutterWindow hosting a Flutter view running |project|.
  explicit FlutterWindow(const flutter::DartProject& project);
  virtual ~FlutterWindow();

 protected:
  // Win32Window:
  bool OnCreate() override;
  void OnDestroy() override;
  LRESULT MessageHandler(HWND window, UINT const message, WPARAM const wparam,
                         LPARAM const lparam) noexcept override;

 private:
  // The project to run.
  flutter::DartProject project_;

  // The Flutter instance hosted by this window.
  std::unique_ptr<flutter::FlutterViewController> flutter_controller_;

  // Channel used to report OS media key presses (registered as global
  // hotkeys, so they work even while the window is minimized/unfocused).
  std::unique_ptr<flutter::MethodChannel<flutter::EncodableValue>>
      media_key_channel_;

  // Channel Dart uses to tell us whether to show the play or pause glyph
  // on the taskbar thumbnail toolbar's middle button.
  std::unique_ptr<flutter::MethodChannel<flutter::EncodableValue>>
      taskbar_channel_;

  // Taskbar thumbnail toolbar (the row of buttons shown when hovering the
  // taskbar icon). Re-created whenever Explorer sends "TaskbarButtonCreated".
  ITaskbarList3* taskbar_list_ = nullptr;
  UINT taskbar_button_created_message_ = 0;
  bool media_playing_ = false;

  void InitializeThumbarButtons();
  void SetThumbarPlaying(bool playing);
};

#endif  // RUNNER_FLUTTER_WINDOW_H_
