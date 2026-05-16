// AImGui: a minimal Dear ImGui Android ARM64 ELF.
#include "core/font.h"
#include "core/ime_bridge.h"
#include "core/keyboard_input.h"
#include "core/renderer.h"
#include "imgui.h"
#include "platform/ANativeWindowCreator.h"
#include "platform/TouchHelperA.h"
#include "ui/ui.h"

#include <chrono>
#include <thread>

namespace {

struct WindowCtx {
    ANativeWindow* window = nullptr;
    std::unique_ptr<aimgui::IRenderer> renderer;
};

bool BuildWindow(WindowCtx* ctx, int W, int H, bool permeate_record) {
    ctx->window = android::ANativeWindowCreator::Create("AImGui", W, W, permeate_record);
    if (!ctx->window) return false;
    ctx->renderer = aimgui::MakeRenderer(ctx->window, W, W, aimgui::Backend::Auto);
    if (!ctx->renderer) {
        android::ANativeWindowCreator::Destroy(ctx->window);
        ctx->window = nullptr;
        return false;
    }
    return true;
}

void DestroyWindow(WindowCtx* ctx) {
    if (ctx->renderer) { ctx->renderer->Shutdown(); ctx->renderer.reset(); }
    if (ctx->window)   { android::ANativeWindowCreator::Destroy(ctx->window); ctx->window = nullptr; }
}

// Drift-corrected frame pacer (see commit 58990ff).
class FramePacer {
public:
    void SetTargetFps(int fps) {
        if (fps == m_TargetFps) return;
        m_TargetFps = fps;
        m_Period    = fps > 0 ? std::chrono::nanoseconds(1'000'000'000LL / fps)
                              : std::chrono::nanoseconds::zero();
        m_NextDeadline = std::chrono::steady_clock::now();
    }
    void Wait() {
        if (m_TargetFps <= 0) return;
        auto now = std::chrono::steady_clock::now();
        m_NextDeadline += m_Period;
        if (m_NextDeadline < now) m_NextDeadline = now + m_Period;
        std::this_thread::sleep_until(m_NextDeadline);
    }
private:
    int m_TargetFps = 0;
    std::chrono::nanoseconds m_Period{};
    std::chrono::steady_clock::time_point m_NextDeadline{};
};

} // namespace

int main() {
    using namespace android;
    using clock = std::chrono::steady_clock;

    auto info = ANativeWindowCreator::GetDisplayInfo();
    const int W = info.width  > info.height ? info.width  : info.height;
    const int H = info.width  > info.height ? info.height : info.width;

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.LogFilename = nullptr;
    io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;

    ImGui::StyleColorsDark();
    aimgui::LoadDefaultAndSystemCJKFont(22.0f);

    aimgui::UiState st;
    st.display_w = info.width;
    st.display_h = info.height;

    WindowCtx ctx;
    if (!BuildWindow(&ctx, W, H, st.permeate_record)) {
        ImGui::DestroyContext();
        return 1;
    }
    st.renderer_name = ctx.renderer->Name();

    Touch::Init({(float)W, (float)H}, false);
    Touch::setOrientation((int)info.orientation);

    aimgui::kbd_input::Init();

    // Optional Java helper for system IME input. If the dex isn't beside
    // the binary (or app_process refuses to launch) we silently carry on
    // without IME — everything else still works.
    aimgui::ime::Init("/data/local/tmp/AImGui.dex");

    bool prev_want_text = false;

    auto last = clock::now();
    bool running = true;
    uint32_t cached_orientation = info.orientation;

    FramePacer pacer;

    while (running) {
        auto frame_start = clock::now();
        io.DeltaTime = std::chrono::duration<float>(frame_start - last).count();
        if (io.DeltaTime <= 0.f) io.DeltaTime = 1.0f / 60.0f;
        last = frame_start;

        pacer.SetTargetFps(st.target_fps);

        info = ANativeWindowCreator::GetDisplayInfo();
        st.display_w = info.width;
        st.display_h = info.height;
        if (info.orientation != cached_orientation) {
            cached_orientation = info.orientation;
            Touch::setOrientation((int)info.orientation);
        }

        // Volume key toggles full ↔ island.
        if (aimgui::kbd_input::ConsumeVolumePresses() > 0) {
            st.collapsed = !st.collapsed;
        }

        if (!st.permeate_record) {
            ANativeWindowCreator::ProcessMirrorDisplay();
        }

        aimgui::kbd_input::Flush();
        aimgui::ime::Flush();

        ctx.renderer->NewFrame();
        ImGui::NewFrame();
        aimgui::DrawUi(&st, &running);
        ctx.renderer->EndFrame();

        // Bring the system IME up / down to follow ImGui's text-input focus.
        const bool want_text = io.WantTextInput;
        if (want_text != prev_want_text) {
            if (want_text) aimgui::ime::Show();
            else           aimgui::ime::Hide();
            prev_want_text = want_text;
        }

        pacer.Wait();

        if (st.request_permeate_toggle) {
            st.request_permeate_toggle = false;
            st.permeate_record = !st.permeate_record;
            DestroyWindow(&ctx);
            if (!BuildWindow(&ctx, W, H, st.permeate_record)) {
                running = false;
                break;
            }
            st.renderer_name = ctx.renderer->Name();
        }
    }

    aimgui::ime::Shutdown();
    aimgui::kbd_input::Shutdown();
    DestroyWindow(&ctx);
    ImGui::DestroyContext();
    return 0;
}
