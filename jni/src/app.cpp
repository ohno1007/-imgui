#include "app.h"

#include "imgui.h"

#include <cstdio>

namespace aimgui {

namespace {
char g_RendererName[32] = {};
}

void AppInit(const char* renderer_name) {
    if (renderer_name) {
        std::snprintf(g_RendererName, sizeof(g_RendererName), "%s", renderer_name);
    }
}

void AppFrame(bool* keep_running) {
    ImGui::SetNextWindowPos(ImVec2(60, 100), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(620, 360), ImGuiCond_FirstUseEver);

    ImGui::Begin(u8"AImGui  -  迷你 ImGui 框架", keep_running);

    ImGui::Text("Dear ImGui %s", ImGui::GetVersion());
    ImGui::Text(u8"渲染后端: %s", g_RendererName[0] ? g_RendererName : "?");
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
    ImGui::TextDisabled(u8"拖动标题栏可移动窗口。");
    ImGui::Text(u8"%.1f FPS  (%.2f 毫秒)",
                ImGui::GetIO().Framerate,
                1000.0f / ImGui::GetIO().Framerate);

    if (ImGui::Button(u8"退出")) *keep_running = false;

    ImGui::End();
}

} // namespace aimgui
