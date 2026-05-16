#pragma once

#include "imgui.h"   // ImVec2

namespace aimgui {

// State shared between main loop and the UI layer. The main loop owns
// the struct, hands a pointer to DrawUi() each frame; UI sets *request_*
// flags, main consumes / clears them next frame.
struct UiState {
    const char* renderer_name = nullptr;

    // Anti-recording: surface created with the skipScreenshot flag so
    // it doesn't show up in screen captures / casts.
    bool permeate_record         = false;
    bool request_permeate_toggle = false;

    // Frame-rate cap. 0 = vsync (panel refresh).
    int target_fps = 0;

    // Current visible display size in ImGui coordinates, so the Dynamic
    // Island can re-center itself on portrait↔landscape rotation.
    int display_w = 0;
    int display_h = 0;

    // ── Dynamic Island ───────────────────────────────────────────────
    // `collapsed` is the *target* state (true = pill at top, false =
    // full window). `expand` is the animated value lerping toward it
    // through a spring; `expand_vel` is the spring's velocity.
    bool  collapsed   = false;
    float expand      = 1.0f;
    float expand_vel  = 0.0f;

    // Remembered full-window pos / size so the window springs back to
    // wherever the user last dragged it.
    ImVec2 last_full_pos  = ImVec2(60, 100);
    ImVec2 last_full_size = ImVec2(900, 620);
};

void DrawUi(UiState* state, bool* keep_running);

} // namespace aimgui
