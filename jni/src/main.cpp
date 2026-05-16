// AImGui: a minimal Dear ImGui Android ARM64 ELF.
#include "core/font.h"
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

// Drift-corrected frame pacer. Holds an absolute next_deadline that
// advances by exactly one period each frame, so wake-up jitter doesn't
// accumulate into a lower-than-target average rate. Resyncs if we fall a
// full period behind (e.g. the user just lowered the target FPS).
class FramePacer {
public:
    void SetTargetFps(int fps) {
        if (fps == m_TargetFps) return;
        m_TargetFps = fps;
        m_Period    = fps > 0 ? std::chrono::nanoseconds(1'000'000'000LL / fps)
                              : std::chrono::nanoseconds::zero();
        m_NextDeadline = std::chrono::steady_clock::now();
    }

    // Block until it's time for the next frame. No-op when uncapped.
    void Wait() {
        if (m_TargetFps <= 0) return;
        auto now = std::chrono::steady_clock::now();
        m_NextDeadline += m_Period;
        if (m_NextDeadline < now) m_NextDeadline = now + m_Period; // resync
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
    ImGui::GetStyle().ScaleAllSizes(2.5f);
    aimgui::LoadDefaultAndSystemCJKFont(22.0f);

    aimgui::UiState st;

    WindowCtx ctx;
    if (!BuildWindow(&ctx, W, H, st.permeate_record)) {
        ImGui::DestroyContext();
        return 1;
    }
    st.renderer_name = ctx.renderer->Name();

    Touch::Init({(float)W, (float)H}, false);
    Touch::setOrientation((int)info.orientation);

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
        if (info.orientation != cached_orientation) {
            cached_orientation = info.orientation;
            Touch::setOrientation((int)info.orientation);
        }
        if (!st.permeate_record) {
            ANativeWindowCreator::ProcessMirrorDisplay();
        }

        ctx.renderer->NewFrame();
        ImGui::NewFrame();
        aimgui::DrawUi(&st, &running);
        ctx.renderer->EndFrame();

        pacer.Wait();

        // ── anti-recording toggle ───────────────────────────────────
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

    DestroyWindow(&ctx);
    ImGui::DestroyContext();
    return 0;
}
