#pragma once

// IME bridge: spawns a JVM via /system/bin/app_process that runs
// com.aimgui.Helper (compiled from java/src/...). The helper hosts a
// hidden EditText attached to WindowManager, drives Android's system IME
// via InputMethodManager.showSoftInput(), and pipes typed characters back
// to us over the child's stdout. We just have to feed those characters
// into io.AddInputCharactersUTF8() each frame.

namespace aimgui::ime {

// Spawns the child if a classes.dex is found at `dex_path`. Returns true
// on success; false if fork/exec failed or the dex doesn't exist.
bool Init(const char* dex_path);

// True after a successful Init() and before Shutdown() / child death.
bool IsRunning();

// True while the system soft keyboard is on-screen, per the helper's
// WindowInsets listener.
bool IsImeVisible();

// Request the system IME to show / hide. No-op when not running.
void Show();
void Hide();

// Drain queued text into ImGui::GetIO(). Call once per frame from the
// main thread, before ImGui::NewFrame().
void Flush();

void Shutdown();

} // namespace aimgui::ime
