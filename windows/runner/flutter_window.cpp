#include "flutter_window.h"

#include <optional>

#include "flutter/generated_plugin_registrant.h"

namespace {
// ponytail: global hotkeys, not SMTC, this only covers play/pause/next/prev,
// add Windows.Media.SystemMediaTransportControls if lock-screen/taskbar
// now-playing metadata is ever needed.
constexpr int kHotKeyPlayPause = 1;
constexpr int kHotKeyNextTrack = 2;
constexpr int kHotKeyPrevTrack = 3;
}  // namespace

FlutterWindow::FlutterWindow(const flutter::DartProject& project)
    : project_(project) {}

FlutterWindow::~FlutterWindow() {}

bool FlutterWindow::OnCreate() {
  if (!Win32Window::OnCreate()) {
    return false;
  }

  RECT frame = GetClientArea();

  // The size here must match the window dimensions to avoid unnecessary surface
  // creation / destruction in the startup path.
  flutter_controller_ = std::make_unique<flutter::FlutterViewController>(
      frame.right - frame.left, frame.bottom - frame.top, project_);
  // Ensure that basic setup of the controller was successful.
  if (!flutter_controller_->engine() || !flutter_controller_->view()) {
    return false;
  }
  RegisterPlugins(flutter_controller_->engine());
  SetChildContent(flutter_controller_->view()->GetNativeWindow());

  media_key_channel_ =
      std::make_unique<flutter::MethodChannel<flutter::EncodableValue>>(
          flutter_controller_->engine()->messenger(), "Echo/media_keys",
          &flutter::StandardMethodCodec::GetInstance());

  RegisterHotKey(GetHandle(), kHotKeyPlayPause, MOD_NOREPEAT, VK_MEDIA_PLAY_PAUSE);
  RegisterHotKey(GetHandle(), kHotKeyNextTrack, MOD_NOREPEAT, VK_MEDIA_NEXT_TRACK);
  RegisterHotKey(GetHandle(), kHotKeyPrevTrack, MOD_NOREPEAT, VK_MEDIA_PREV_TRACK);

  flutter_controller_->engine()->SetNextFrameCallback([&]() {
    this->Show();
  });

  // Flutter can complete the first frame before the "show window" callback is
  // registered. The following call ensures a frame is pending to ensure the
  // window is shown. It is a no-op if the first frame hasn't completed yet.
  flutter_controller_->ForceRedraw();

  return true;
}

void FlutterWindow::OnDestroy() {
  UnregisterHotKey(GetHandle(), kHotKeyPlayPause);
  UnregisterHotKey(GetHandle(), kHotKeyNextTrack);
  UnregisterHotKey(GetHandle(), kHotKeyPrevTrack);

  if (flutter_controller_) {
    flutter_controller_ = nullptr;
  }

  Win32Window::OnDestroy();
}

LRESULT
FlutterWindow::MessageHandler(HWND hwnd, UINT const message,
                              WPARAM const wparam,
                              LPARAM const lparam) noexcept {
  // Give Flutter, including plugins, an opportunity to handle window messages.
  if (flutter_controller_) {
    std::optional<LRESULT> result =
        flutter_controller_->HandleTopLevelWindowProc(hwnd, message, wparam,
                                                      lparam);
    if (result) {
      return *result;
    }
  }

  switch (message) {
    case WM_FONTCHANGE:
      flutter_controller_->engine()->ReloadSystemFonts();
      break;
    case WM_HOTKEY:
      if (media_key_channel_) {
        switch (wparam) {
          case kHotKeyPlayPause:
            media_key_channel_->InvokeMethod("playPause", nullptr);
            break;
          case kHotKeyNextTrack:
            media_key_channel_->InvokeMethod("next", nullptr);
            break;
          case kHotKeyPrevTrack:
            media_key_channel_->InvokeMethod("previous", nullptr);
            break;
        }
      }
      break;
  }

  return Win32Window::MessageHandler(hwnd, message, wparam, lparam);
}
