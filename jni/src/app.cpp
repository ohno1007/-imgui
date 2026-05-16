#include "app.h"

#include "imgui.h"

namespace aimgui {

void AppFrame(AppState* state, bool* keep_running) {
    ImGui::SetNextWindowPos(ImVec2(60, 100), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(620, 380), ImGuiCond_FirstUseEver);

    ImGui::Begin("AImGui  -  minimal ImGui framework", keep_running);

    ImGui::Text("Dear ImGui %s", ImGui::GetVersion());
    ImGui::Text("renderer: %s", state->renderer_name ? state->renderer_name : "?");
    ImGui::Separator();

    static int counter = 0;
    static float slider = 0.5f;
    static bool toggle = false;
    static ImVec4 tint(0.4f, 0.7f, 1.0f, 1.0f);

    if (ImGui::Button("tap me")) counter++;
    ImGui::SameLine();
    ImGui::Text("count = %d", counter);

    ImGui::SliderFloat("slider", &slider, 0.0f, 1.0f);
    ImGui::Checkbox("toggle", &toggle);
    ImGui::ColorEdit4("color", (float*)&tint);

    ImGui::Spacing();

    // Anti-recording toggle. Flipping it asks the main loop to rebuild the
    // SurfaceFlinger window with the secure / skipScreenshot flag.
    bool perm = state->permeate_record;
    if (ImGui::Checkbox("anti-recording (hide from screen capture / cast)", &perm)) {
        state->request_permeate_toggle = true;
    }
    ImGui::SameLine();
    ImGui::TextDisabled("%s", state->permeate_record ? "on" : "off");

    ImGui::Spacing();
    ImGui::TextDisabled("Drag the title bar to move the window.");
    ImGui::Text("%.1f FPS  (%.2f ms)",
                ImGui::GetIO().Framerate,
                1000.0f / ImGui::GetIO().Framerate);

    if (ImGui::Button("exit")) *keep_running = false;

    ImGui::End();
}

} // namespace aimgui
