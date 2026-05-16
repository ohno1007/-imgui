#pragma once

namespace aimgui {

// State shared between main loop and the UI layer. The main loop owns the
// struct, hands a pointer to DrawUi() each frame; UI sets *request_* flags,
// main consumes / clears them next frame.
struct UiState {
    const char* renderer_name = nullptr;

    // Anti-recording: surface created with the skipScreenshot flag so it
    // doesn't show up in screen captures / casts.
    bool permeate_record         = false;  // current state
    bool request_permeate_toggle = false;  // flip request

    // Frame-rate cap. 0 = unlimited (only the panel refresh / present mode
    // can bound us). UI exposes a small set of presets via DrawUi().
    int target_fps = 0;
};

void DrawUi(UiState* state, bool* keep_running);

} // namespace aimgui
