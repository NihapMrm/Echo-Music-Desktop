# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project

Echo Music Desktop — a Flutter desktop app (Windows/macOS/Linux) that streams from YouTube Music ad-free. It's a fork of `EchoMusicApp/Echo-Music-Desktop`; see [GIT_WORKFLOW.md](GIT_WORKFLOW.md) for the fork/upstream remote setup (`origin` = fork, `upstream` = original — fetch/merge from `upstream`, push to `origin`).

## Commands

```bash
flutter pub get                       # install dependencies
flutter run -d windows|macos|linux    # run the app
flutter build windows|macos|linux     # build a release binary
flutter analyze                       # lint (flutter_lints, see analysis_options.yaml)
flutter test                          # run tests (test/widget_test.dart)
flutter test test/widget_test.dart    # run a single test file
```

Desktop support must be enabled once per machine: `flutter config --enable-windows-desktop` (and `-macos-desktop` / `-linux-desktop`).

Linux distribution packages (DEB/AppImage/RPM) cannot be built on macOS — see [BUILDING_LINUX.md](BUILDING_LINUX.md) for `flutter_distributor` usage and the GitHub Actions workflow.

**Windows installer**: `flutter_distributor release --name dev --jobs release-dev-windows-exe` (config in `distribute_options.yaml` + `windows/packaging/exe/make_config.yaml`). Needs Inno Setup; if installed somewhere other than the default `C:\Program Files (x86)\Inno Setup 6`, set the `INNO_SETUP_PATH` user env var to its install dir. The `app_id` GUID in `make_config.yaml` must never change — it's what lets a new installer upgrade-in-place (same Add/Remove Programs entry) instead of creating a duplicate install. User data (Hive boxes, downloads) lives outside the install dir (`%APPDATA%`, `Downloads\Echo`), so installer upgrades never lose it.

Localization (`lib/generated/`, `lib/l10n/*.arb`) is generated via `flutter_intl`/`intl_utils` (`generate: true` in pubspec.yaml) — regenerate with the Flutter Intl tooling rather than hand-editing `lib/generated/intl/*`.

## Release & update workflow

1. **Dev loop**: `git fetch upstream && git merge upstream/main` to pull upstream changes (see [GIT_WORKFLOW.md](GIT_WORKFLOW.md)); `flutter run -d windows` to test; commit and `git push origin main`.
2. **Cutting a release**: bump the version in `pubspec.yaml` *and* in [desktop_update.json](desktop_update.json) (must match), rebuild the installer (Commands above), tag and push, then create a GitHub Release on the fork with the new installer attached.
3. **How users get notified**: `UpdateService.autoCheck` (called from `AppShell.initState`) fetches `desktop_update.json` from `raw.githubusercontent.com/<owner>/<repo>/main/` (constants in `lib/services/update_service/update_service.dart`) and compares its `version` field against `PackageInfo.fromPlatform()`. Any mismatch (not just "newer") triggers the update dialog, whose button opens `github.com/<owner>/<repo>/releases/latest` — there is no in-app auto-download, the user re-runs the new installer manually.
4. **Why updates don't lose data**: see the `app_id` note under Commands — installer upgrades only touch the install dir, never `%APPDATA%` or `Downloads\Echo`.

## Architecture

**DI**: `GetIt` (`lib/core/utils/service_locator.dart` exposes the global `sl`, but most call sites use `GetIt.I<T>()` directly). Singletons — `SettingsManager`, `MediaPlayer`, `LibraryService`, `YTMusic`, `FileStorage`, `DownloadManager`, `LyricsService` — are registered in `lib/main.dart` at startup, before `runApp`.

**State management is mixed by layer**:
- App-wide cross-cutting state (`SettingsManager`, `MediaPlayer`, `LibraryService`) is `ChangeNotifier` + `provider`, wired in the `MultiProvider` in `main.dart`.
- Per-screen state uses `flutter_bloc` Cubits, one `cubit/` folder beside each screen (e.g. `lib/screens/home/cubit/home_cubit.dart` + `home_state.dart`). New screens should follow this same `screen_page.dart` + `cubit/screen_cubit.dart` + `cubit/screen_state.dart` pattern.

**Persistence**: Hive boxes opened once in `main.dart`'s `initialiseHive()` — `SETTINGS`, `LIBRARY`, `SEARCH_HISTORY`, `SONG_HISTORY`, `FAVOURITES`, `DOWNLOADS`, `LOCAL_MUSIC`. Services wrap a box and call `notifyListeners()` on the box's `listenable()` (see `lib/services/library.dart`) so UI reacts to Hive changes without manual events. `LocalLibraryService` (`lib/services/local_library.dart`) is the odd one out: it indexes the user's own audio files (key = file path, so re-import is a no-op and removal never touches the file) and feeds them into `MediaPlayer` the same way a downloaded song would (`status: 'DOWNLOADED'`, no `thumbnails`) — kept off the shared `SongTile`/`ItemList` widgets, which assume network artwork, in its own `LocalMusicPage`.

**YouTube Music API client** (`lib/ytmusic/`): `YTMusic` (`ytmusic.dart`) composes `YTClient` (`client.dart`) with mixins — `BrowsingMixin`, `LibraryMixin`, `SearchMixin` (`mixins/*.dart`) — each contributing a slice of the API surface onto one class. `YTMusicServices` (`yt_service_provider.dart`) is the abstract base handling headers, visitor ID, and the raw `sendRequest`/`sendGetRequest` calls to `music.youtube.com`'s internal `youtubei` API. When adding new YT Music endpoints, add a method to the relevant mixin rather than growing `YTClient` directly.

**Audio playback**: `MediaPlayer` (`lib/services/media_player.dart`) wraps `just_audio`'s `AudioPlayer`, exposing `ValueNotifier`s for current song/index/loop/progress instead of raw streams, for simpler widget consumption. Actual audio bytes come from `YouTubeAudioSource` (`lib/services/yt_audio_stream.dart`), a `StreamAudioSource` that resolves a YouTube stream URL on demand via `youtube_explode_dart` (with platform-specific container preference: mp4/m4a preferred on macOS/iOS). `media_kit` backs playback on Windows/Linux/macOS (`JustAudioMediaKit`, initialized in `main.dart`); mobile uses `just_audio`'s default backend. A local HTTP server (`createAudioStreamServer`, started in `main.dart`) fronts audio streaming.

**Routing**: `go_router` (`lib/utils/router.dart`) with a `StatefulShellRoute` for the four bottom-nav tabs (Home, Search, Library, Settings), each a `StatefulShellBranch`. `/player` is a top-level route outside the shell, pushed as a custom slide-up transition. Nested routes under each branch take args via `state.extra` (typed `Map<String, dynamic>` or domain objects) rather than path/query params.

**Cross-platform UI**: `lib/utils/adaptive_widgets/` provides platform-adaptive wrappers (buttons, switches, text fields, etc.) — prefer these over raw Material/Cupertino widgets when adding UI that should look native on both desktop platforms.

**Windows native integration** (`windows/runner/flutter_window.cpp`/`.h`): Flutter only receives keyboard events while the window has OS focus, so OS-level media keys, the taskbar thumbnail toolbar, and the system tray icon are all implemented in native Win32 code (no plugin dependency) and forwarded to Dart over two `MethodChannel`s — `Echo/media_keys` (play/pause/next/previous, called from `RegisterHotKey`, `WM_COMMAND` thumbbar clicks, and the tray context menu, which all reuse the same three command ids) and `Echo/taskbar` (Dart → native, keeps the thumbbar/tray play-pause glyph in sync with `MediaPlayer.buttonState`). Close (✕) hides to tray (`WM_CLOSE` interception); minimize behaves normally; only the tray menu's "Exit" actually quits (`DestroyWindow` directly, bypassing the `WM_CLOSE` handler). Both `main.dart`'s media-key/taskbar channel handlers and this native code need updating together if the IPC contract changes.

**Global floating volume control** (`lib/core/widgets/floating_volume_control.dart`): mounted once via `MaterialApp.router`'s `builder` in `main.dart`, wrapped in its own `Overlay` — needed because the builder's `child` already contains the Router's Overlay, but a sibling widget added in `builder` is not a descendant of it, and Material's `Slider` needs an `Overlay` ancestor.

**Package name caveat**: the Dart package name is `Echo` (capitalized, per `pubspec.yaml`), so internal imports are `package:Echo/...`, not `package:echo/...`.
