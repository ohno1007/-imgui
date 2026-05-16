#pragma once

#include <cstddef>

// Bridge to Termux:API's `termux-dialog text` command.
//
// On Android, native ELFs can't receive characters from a system IME
// (the IME → app channel needs a JVM-side InputConnection). Termux:API,
// when installed alongside Termux, exposes a `termux-dialog text` shell
// command that pops up an Android dialog with an EditText, captures the
// user's typed string (any IME works here — system soft keyboard, voice,
// handwriting, …) and emits the result as JSON on stdout.
//
// We popen() that command from a worker thread, parse the JSON, and copy
// the resulting text back into the caller's char buffer on the main
// thread next frame.
//
// Requirements: Termux and Termux:API APKs installed, our process able
// to exec into /data/data/com.termux/files/usr/bin/.

namespace aimgui::termux_input {

void Init();
void Shutdown();

// Call once per frame on the main thread. Drains a completed dialog's
// text into whichever buffer was registered via Launch().
void Tick();

// True while a dialog is currently showing (worker thread is in popen()).
bool IsBusy();

// True once the device looks like it has Termux:API available.
bool IsAvailable();

// Spawns a dialog. `dest_buf` will be filled with the typed text on the
// next Tick() that runs after the user confirms. Pass the InputText's
// own buffer pointer + capacity. Caller must keep the buffer alive at
// least until the next Tick(); for static buffers (the usual case)
// that's trivially satisfied.
void Launch(char* dest_buf, std::size_t dest_buf_size,
            const char* title = "AImGui",
            const char* hint  = nullptr);

} // namespace aimgui::termux_input
