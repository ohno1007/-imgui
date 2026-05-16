#include "ui/ui.h"

#include "imgui.h"

namespace aimgui {

namespace {

constexpr int   kFpsPresets[] = { 0,   30,  60,  90, 120, 144 };
constexpr const char* kFpsLabels =
    "vsync\0" "30\0" "60\0" "90\0" "120\0" "144\0";

int FpsToIndex(int target_fps) {
    for (int i = 0; i < IM_ARRAYSIZE(kFpsPresets); ++i)
        if (kFpsPresets[i] == target_fps) return i;
    return 0;
}

} // namespace

void DrawUi(UiState* state, bool* keep_running) {
    ImGui::SetNextWindowPos(ImVec2(60, 100),  ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(620, 420), ImGuiCond_FirstUseEver);

    ImGui::Begin("AImGui  -  minimal ImGui framework", keep_running);

    ImGui::Text("Dear ImGui %s", ImGui::GetVersion());
    ImGui::Text("renderer: %s", state->renderer_name ? state->renderer_name : "?");
    ImGui::Separator();

    // ── frame-rate control ────────────────────────────────────────────
    int fps_idx = FpsToIndex(state->target_fps);
    if (ImGui::Combo("frame rate", &fps_idx, kFpsLabels)) {
        state->target_fps = kFpsPresets[fps_idx];
    }
    ImGui::SameLine();
    ImGui::TextDisabled("now: %.1f FPS  (%.2f ms)",
                        ImGui::GetIO().Framerate,
                        1000.0f / ImGui::GetIO().Framerate);

    // ── playground widgets ────────────────────────────────────────────
    static int    counter = 0;
    static float  slider  = 0.5f;
    static bool   toggle  = false;
    static ImVec4 tint(0.4f, 0.7f, 1.0f, 1.0f);

    if (ImGui::Button("tap me")) counter++;
    ImGui::SameLine();
    ImGui::Text("count = %d", counter);

    ImGui::SliderFloat("slider", &slider, 0.0f, 1.0f);
    ImGui::Checkbox("toggle", &toggle);
    ImGui::ColorEdit4("color", (float*)&tint);

    // ── anti-recording toggle ─────────────────────────────────────────
    ImGui::Spacing();
    bool perm = state->permeate_record;
    if (ImGui::Checkbox("anti-recording (hide from capture / cast)", &perm)) {
        state->request_permeate_toggle = true;
    }
    ImGui::SameLine();
    ImGui::TextDisabled("%s", state->permeate_record ? "on" : "off");

    ImGui::Spacing();
    ImGui::TextDisabled("Drag the title bar to move the window.");
    if (ImGui::Button("exit")) *keep_running = false;

    ImGui::End();
}

} // namespace aimgui
