#include "ui/ui.h"

#include "imgui.h"

#include <cstdio>

namespace aimgui {

namespace {

// ─── Navigation pages ────────────────────────────────────────────────────
enum class Page { Dashboard, Widgets, Window, Performance, About };

struct PageItem { Page id; const char* label; };
constexpr PageItem kPages[] = {
    { Page::Dashboard,   "Dashboard"   },
    { Page::Widgets,     "Widgets"     },
    { Page::Window,      "Window"      },
    { Page::Performance, "Performance" },
    { Page::About,       "About"       },
};

// ─── Frame-rate presets ──────────────────────────────────────────────────
constexpr int kFpsPresets[] = { 0, 30, 60, 90, 120, 144 };
constexpr const char* kFpsLabels =
    "vsync\0" "30\0" "60\0" "90\0" "120\0" "144\0";

int FpsToIndex(int fps) {
    for (int i = 0; i < IM_ARRAYSIZE(kFpsPresets); ++i)
        if (kFpsPresets[i] == fps) return i;
    return 0;
}

// ─── One-time style polish ───────────────────────────────────────────────
void ApplyStyleOnce() {
    static bool done = false;
    if (done) return;
    done = true;
    auto& s = ImGui::GetStyle();
    // Touch-sized values tuned for a 22 px font; replace ImGui's defaults
    // entirely (main.cpp leaves them at base scale).
    s.WindowRounding          = 12.0f;
    s.ChildRounding           = 10.0f;
    s.FrameRounding           = 6.0f;
    s.GrabRounding            = 6.0f;
    s.PopupRounding           = 6.0f;
    s.ScrollbarRounding       = 10.0f;
    s.WindowBorderSize        = 1.0f;
    s.FrameBorderSize         = 0.0f;
    s.WindowPadding           = ImVec2(0, 0);     // sidebar / content do their own
    s.ItemSpacing             = ImVec2(14, 12);
    s.ItemInnerSpacing        = ImVec2(8, 6);
    s.FramePadding            = ImVec2(14, 10);
    s.ScrollbarSize           = 18.0f;
    s.GrabMinSize             = 16.0f;
    s.SeparatorTextBorderSize = 3.0f;
    s.SeparatorTextPadding    = ImVec2(28, 8);
}

// ─── Key/value row helper ────────────────────────────────────────────────
void KV(const char* key, const char* fmt, ...) {
    ImGui::TextDisabled("%s", key);
    ImGui::SameLine(160.0f);
    va_list args;
    va_start(args, fmt);
    ImGui::TextV(fmt, args);
    va_end(args);
}

// ─── Page contents ───────────────────────────────────────────────────────
void DrawDashboard(const UiState* state) {
    ImGui::SeparatorText("Overview");

    KV("renderer",      "%s", state->renderer_name ? state->renderer_name : "?");
    KV("ImGui",         "%s", ImGui::GetVersion());
    KV("frame",         "%.1f FPS  (%.2f ms)",
                         ImGui::GetIO().Framerate,
                         1000.0f / ImGui::GetIO().Framerate);
    KV("anti-recording", "%s", state->permeate_record ? "on" : "off");

    ImGui::Spacing();
    ImGui::SeparatorText("Hint");
    ImGui::TextWrapped(
        "Pick a section in the left panel to tweak settings or play with "
        "the widgets. Drag the title bar to move this window.");
}

void DrawWidgets() {
    ImGui::SeparatorText("Basic widgets");

    static int    counter = 0;
    static float  slider  = 0.5f;
    static bool   toggle  = false;
    static ImVec4 tint(0.40f, 0.70f, 1.00f, 1.0f);
    static char   text[64] = "Hello, AImGui";

    if (ImGui::Button("tap me")) counter++;
    ImGui::SameLine();
    ImGui::Text("count = %d", counter);

    ImGui::InputText("text",   text, IM_ARRAYSIZE(text));
    ImGui::SliderFloat("slider", &slider, 0.0f, 1.0f, "%.3f");
    ImGui::Checkbox("toggle",  &toggle);
    ImGui::ColorEdit4("color", (float*)&tint);

    ImGui::Spacing();
    ImGui::SeparatorText("List");
    static int selected = -1;
    for (int i = 0; i < 4; ++i) {
        char label[32];
        std::snprintf(label, sizeof(label), "item %d", i);
        if (ImGui::Selectable(label, selected == i)) selected = i;
    }

    ImGui::Spacing();
    ImGui::SeparatorText("Progress");
    static float progress = 0.0f;
    progress += ImGui::GetIO().DeltaTime * 0.15f;
    if (progress > 1.0f) progress -= 1.0f;
    ImGui::ProgressBar(progress, ImVec2(-1, 0));
}

void DrawWindow(UiState* state) {
    ImGui::SeparatorText("Surface");

    bool perm = state->permeate_record;
    if (ImGui::Checkbox("anti-recording (hide from capture / cast)", &perm)) {
        state->request_permeate_toggle = true;
    }
    ImGui::SameLine();
    ImGui::TextDisabled("[%s]", state->permeate_record ? "on" : "off");

    ImGui::TextWrapped(
        "When on, the SurfaceFlinger layer is created with the secure / "
        "skipScreenshot flag, so screen recordings and casts won't see it.");

    ImGui::Spacing();
    ImGui::SeparatorText("Theme");
    static int theme = 0;
    if (ImGui::Combo("##theme", &theme, "dark\0light\0classic\0")) {
        switch (theme) {
            case 0: ImGui::StyleColorsDark();    break;
            case 1: ImGui::StyleColorsLight();   break;
            case 2: ImGui::StyleColorsClassic(); break;
        }
    }
}

void DrawPerformance(UiState* state) {
    ImGui::SeparatorText("Frame-rate cap");

    int fps_idx = FpsToIndex(state->target_fps);
    if (ImGui::Combo("target", &fps_idx, kFpsLabels)) {
        state->target_fps = kFpsPresets[fps_idx];
    }

    ImGui::Spacing();
    KV("current", "%.1f FPS", ImGui::GetIO().Framerate);
    KV("frame time", "%.2f ms", 1000.0f / ImGui::GetIO().Framerate);

    ImGui::Spacing();
    ImGui::SeparatorText("Rolling FPS");

    // Sliding window of recent FPS samples.
    constexpr int N = 240;
    static float history[N] = {};
    static int   offset     = 0;
    history[offset] = ImGui::GetIO().Framerate;
    offset = (offset + 1) % N;

    char overlay[32];
    std::snprintf(overlay, sizeof(overlay), "%.1f", ImGui::GetIO().Framerate);
    ImGui::PlotLines("##fps_plot", history, N, offset, overlay,
                     0.0f, 165.0f, ImVec2(-1, 90));
}

void DrawAbout() {
    ImGui::SeparatorText("AImGui");
    ImGui::TextWrapped("A minimal Dear ImGui Android ARM64 ELF — no JNI, no APK, "
                       "no Activity. Runs as a native binary on top of "
                       "SurfaceFlinger.");

    ImGui::Spacing();
    ImGui::SeparatorText("Features");
    ImGui::BulletText("Dear ImGui %s", ImGui::GetVersion());
    ImGui::BulletText("Vulkan + OpenGL ES 3 auto-fallback");
    ImGui::BulletText("VSync-locked frame pacer, near-zero CPU spin");
    ImGui::BulletText("Observe-only touch (no /dev/uinput, system unaffected)");
    ImGui::BulletText("Anti-recording surface toggle");

    ImGui::Spacing();
    ImGui::SeparatorText("Layout");
    ImGui::BulletText("libimgui.a            vendored ImGui + backends");
    ImGui::BulletText("libaimgui_platform.a  window + touch glue");
    ImGui::BulletText("libaimgui_core.a      renderer + frame pacing");
    ImGui::BulletText("src/ui/ui.cpp         this UI (linked directly)");
}

void DrawSidebar(Page& current, bool* keep_running) {
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,         ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,           ImVec2(0, 4));
    ImGui::PushStyleVar(ImGuiStyleVar_SelectableTextAlign,   ImVec2(0.0f, 0.5f));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,          ImVec2(18, 6));

    ImGui::BeginChild("##sidebar", ImVec2(190, 0),
                      ImGuiChildFlags_Borders | ImGuiChildFlags_AlwaysUseWindowPadding);

    // Title block
    ImGui::Dummy(ImVec2(0, 6));
    ImGui::SetCursorPosX(18);
    ImGui::TextDisabled("A I M G U I");
    ImGui::Dummy(ImVec2(0, 6));
    ImGui::Separator();
    ImGui::Dummy(ImVec2(0, 8));

    // Nav items
    for (const auto& p : kPages) {
        bool selected = (current == p.id);
        if (ImGui::Selectable(p.label, selected, 0, ImVec2(0, 36))) {
            current = p.id;
        }
    }

    // Bottom exit button
    const float footer_h = 60.0f;
    float remaining = ImGui::GetWindowHeight() - ImGui::GetCursorPosY() - footer_h;
    if (remaining > 0) ImGui::Dummy(ImVec2(0, remaining));
    ImGui::Separator();
    ImGui::Dummy(ImVec2(0, 6));
    if (ImGui::Button("exit", ImVec2(-1, 0))) *keep_running = false;

    ImGui::EndChild();

    ImGui::PopStyleVar(4);
}

void DrawContent(UiState* state, Page page) {
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(20, 16));

    ImGui::BeginChild("##content", ImVec2(0, 0),
                      ImGuiChildFlags_AlwaysUseWindowPadding);
    switch (page) {
        case Page::Dashboard:   DrawDashboard(state);   break;
        case Page::Widgets:     DrawWidgets();          break;
        case Page::Window:      DrawWindow(state);      break;
        case Page::Performance: DrawPerformance(state); break;
        case Page::About:       DrawAbout();            break;
    }
    ImGui::EndChild();

    ImGui::PopStyleVar();
}

} // namespace

void DrawUi(UiState* state, bool* keep_running) {
    ApplyStyleOnce();

    ImGui::SetNextWindowPos (ImVec2(60, 100),  ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(860, 520), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSizeConstraints(ImVec2(640, 380), ImVec2(FLT_MAX, FLT_MAX));

    if (ImGui::Begin("AImGui", keep_running, ImGuiWindowFlags_NoCollapse)) {
        static Page page = Page::Dashboard;
        DrawSidebar(page, keep_running);
        ImGui::SameLine(0, 0);
        DrawContent(state, page);
    }
    ImGui::End();
}

} // namespace aimgui
