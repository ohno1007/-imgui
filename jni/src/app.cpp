#include "app.h"

#include "imgui.h"

namespace aimgui {

void AppFrame(AppState* state, bool* keep_running) {
    ImGui::SetNextWindowPos(ImVec2(60, 100), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(620, 380), ImGuiCond_FirstUseEver);

    ImGui::Begin(u8"AImGui  -  迷你 ImGui 框架", keep_running);

    ImGui::Text("Dear ImGui %s", ImGui::GetVersion());
    ImGui::Text(u8"渲染后端: %s", state->renderer_name ? state->renderer_name : "?");
    ImGui::Separator();

    static int counter = 0;
    static float slider = 0.5f;
    static bool toggle = false;
    static ImVec4 tint(0.4f, 0.7f, 1.0f, 1.0f);

    if (ImGui::Button(u8"点我")) counter++;
    ImGui::SameLine();
    ImGui::Text(u8"计数 = %d", counter);

    ImGui::SliderFloat(u8"滑块", &slider, 0.0f, 1.0f);
    ImGui::Checkbox(u8"开关", &toggle);
    ImGui::ColorEdit4(u8"取色器", (float*)&tint);

    ImGui::Spacing();

    // Anti-recording toggle. Flipping it asks main loop to rebuild the
    // SurfaceFlinger window with the secure / skipScreenshot flag.
    bool perm = state->permeate_record;
    if (ImGui::Checkbox(u8"防录屏 (穿透屏幕录制 / 投屏)", &perm)) {
        state->request_permeate_toggle = true;
    }
    ImGui::SameLine();
    ImGui::TextDisabled(u8"%s", state->permeate_record ? u8"已开启" : u8"已关闭");

    ImGui::Spacing();
    ImGui::TextDisabled(u8"拖动标题栏可移动窗口。");
    ImGui::Text(u8"%.1f FPS  (%.2f 毫秒)",
                ImGui::GetIO().Framerate,
                1000.0f / ImGui::GetIO().Framerate);

    if (ImGui::Button(u8"退出")) *keep_running = false;

    ImGui::End();
}

} // namespace aimgui
