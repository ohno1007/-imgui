#pragma once

namespace aimgui {

struct AppState {
    const char* renderer_name = nullptr;
    bool permeate_record = false;          // current state (read-only for app)
    bool request_permeate_toggle = false;  // app sets; main consumes & resets
};

void AppFrame(AppState* state, bool* keep_running);

} // namespace aimgui
