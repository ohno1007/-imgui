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

// Sleep until `deadline`, or return immediately if it's already past. Uses
// a coarse sleep for the bulk and a short spin for the last ~500µs so the
// FPS cap is precise without burning CPU.
void SleepUntil(std::chrono::steady_clock::time_point deadline) {
    using namespace std::chrono;
    auto now = steady_clock::now();
    if (deadline <= now) return;
    auto remaining = deadline - now;
    if (remaining > microseconds(500)) {
        std::this_thread::sleep_for(remaining - microseconds(500));
    }
    while (steady_clock::now() < deadline) {
        std::this_thread::yield();
    }
}

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

    while (running) {
        auto frame_start = clock::now();
        io.DeltaTime = std::chrono::duration<float>(frame_start - last).count();
        if (io.DeltaTime <= 0.f) io.DeltaTime = 1.0f / 60.0f;
        last = frame_start;

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

        // ── frame-rate cap ──────────────────────────────────────────
        if (st.target_fps > 0) {
            auto period = std::chrono::nanoseconds(1'000'000'000LL / st.target_fps);
            SleepUntil(frame_start + period);
        }

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
