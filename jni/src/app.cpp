#include "app.h"

#include "imgui.h"

namespace aimgui {

void AppInit() {
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.LogFilename = nullptr;
    io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;

    ImGui::StyleColorsDark();
    ImGui::GetStyle().ScaleAllSizes(2.5f);

    ImFontConfig cfg;
    cfg.SizePixels = 22.0f;
    cfg.OversampleH = cfg.OversampleV = 1;
    io.Fonts->AddFontDefault(&cfg);
}

void AppFrame(bool* keep_running) {
    ImGui::SetNextWindowPos(ImVec2(60, 100), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(560, 320), ImGuiCond_FirstUseEver);

    ImGui::Begin("AImGui", keep_running);

    ImGui::Text("Dear ImGui %s  -  OpenGL ES 3", ImGui::GetVersion());
    ImGui::Separator();

    static int counter = 0;
    static float slider = 0.5f;
    static bool toggle = false;
    static ImVec4 tint(0.4f, 0.7f, 1.0f, 1.0f);

    if (ImGui::Button("Tap me")) counter++;
    ImGui::SameLine();
    ImGui::Text("count = %d", counter);

    ImGui::SliderFloat("slider", &slider, 0.0f, 1.0f);
    ImGui::Checkbox("toggle", &toggle);
    ImGui::ColorEdit4("color", (float*)&tint);

    ImGui::Spacing();
    ImGui::TextDisabled("Drag the title bar to move the window.");
    ImGui::Text("%.1f FPS  (%.2f ms)", ImGui::GetIO().Framerate, 1000.0f / ImGui::GetIO().Framerate);

    if (ImGui::Button("Exit")) *keep_running = false;

    ImGui::End();
}

} // namespace aimgui
