#include "flutter_window.h"

#include <optional>

#include "flutter/generated_plugin_registrant.h"
#include "resource.h"

namespace {
// ponytail: global hotkeys, not SMTC, this only covers play/pause/next/prev,
// add Windows.Media.SystemMediaTransportControls if lock-screen/taskbar
// now-playing metadata is ever needed.
constexpr int kHotKeyPlayPause = 1;
constexpr int kHotKeyNextTrack = 2;
constexpr int kHotKeyPrevTrack = 3;

// Thumbnail toolbar button ids (also doubles as their WM_COMMAND id).
constexpr UINT kThumbBtnPrev = 1;
constexpr UINT kThumbBtnPlayPause = 2;
constexpr UINT kThumbBtnNext = 3;
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

  taskbar_channel_ =
      std::make_unique<flutter::MethodChannel<flutter::EncodableValue>>(
          flutter_controller_->engine()->messenger(), "Echo/taskbar",
          &flutter::StandardMethodCodec::GetInstance());
  taskbar_channel_->SetMethodCallHandler(
      [this](const flutter::MethodCall<flutter::EncodableValue>& call,
             std::unique_ptr<flutter::MethodResult<flutter::EncodableValue>>
                 result) {
        if (call.method_name() == "setPlaying") {
          const auto* playing = std::get_if<bool>(call.arguments());
          SetThumbarPlaying(playing != nullptr && *playing);
        }
        result->Success();
      });

  taskbar_button_created_message_ = RegisterWindowMessage(L"TaskbarButtonCreated");

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

  if (taskbar_list_) {
    taskbar_list_->Release();
    taskbar_list_ = nullptr;
  }

  if (flutter_controller_) {
    flutter_controller_ = nullptr;
  }

  Win32Window::OnDestroy();
}

namespace {
HICON LoadThumbIcon(int resource_id) {
  return static_cast<HICON>(LoadImage(GetModuleHandle(nullptr),
                                      MAKEINTRESOURCE(resource_id), IMAGE_ICON,
                                      GetSystemMetrics(SM_CXSMICON),
                                      GetSystemMetrics(SM_CYSMICON), 0));
}

THUMBBUTTON MakeThumbButton(UINT id, HICON icon, const wchar_t* tooltip) {
  THUMBBUTTON button = {};
  button.dwMask = THB_ICON | THB_TOOLTIP | THB_FLAGS;
  button.iId = id;
  button.hIcon = icon;
  button.dwFlags = THBF_ENABLED;
  wcscpy_s(button.szTip, tooltip);
  return button;
}
}  // namespace

void FlutterWindow::InitializeThumbarButtons() {
  if (taskbar_list_) {
    taskbar_list_->Release();
    taskbar_list_ = nullptr;
  }
  if (FAILED(CoCreateInstance(CLSID_TaskbarList, nullptr, CLSCTX_INPROC_SERVER,
                              IID_ITaskbarList3,
                              reinterpret_cast<void**>(&taskbar_list_))) ||
      FAILED(taskbar_list_->HrInit())) {
    taskbar_list_ = nullptr;
    return;
  }

  THUMBBUTTON buttons[3] = {
      MakeThumbButton(kThumbBtnPrev, LoadThumbIcon(IDI_THUMB_PREV), L"Previous"),
      MakeThumbButton(kThumbBtnPlayPause,
                      LoadThumbIcon(media_playing_ ? IDI_THUMB_PAUSE
                                                    : IDI_THUMB_PLAY),
                      L"Play/Pause"),
      MakeThumbButton(kThumbBtnNext, LoadThumbIcon(IDI_THUMB_NEXT), L"Next"),
  };
  taskbar_list_->ThumbBarAddButtons(GetHandle(), 3, buttons);
}

void FlutterWindow::SetThumbarPlaying(bool playing) {
  media_playing_ = playing;
  if (!taskbar_list_) return;
  THUMBBUTTON button = MakeThumbButton(
      kThumbBtnPlayPause,
      LoadThumbIcon(playing ? IDI_THUMB_PAUSE : IDI_THUMB_PLAY),
      L"Play/Pause");
  taskbar_list_->ThumbBarUpdateButtons(GetHandle(), 1, &button);
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

  if (taskbar_button_created_message_ != 0 &&
      message == taskbar_button_created_message_) {
    InitializeThumbarButtons();
    return 0;
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
    case WM_COMMAND:
      if (HIWORD(wparam) == THBN_CLICKED && media_key_channel_) {
        switch (LOWORD(wparam)) {
          case kThumbBtnPrev:
            media_key_channel_->InvokeMethod("previous", nullptr);
            break;
          case kThumbBtnPlayPause:
            media_key_channel_->InvokeMethod("playPause", nullptr);
            break;
          case kThumbBtnNext:
            media_key_channel_->InvokeMethod("next", nullptr);
            break;
        }
      }
      break;
  }

  return Win32Window::MessageHandler(hwnd, message, wparam, lparam);
}
