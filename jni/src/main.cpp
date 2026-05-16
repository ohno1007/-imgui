// AImGui: a minimal Dear ImGui Android ARM64 ELF.
#include "app.h"
#include "core/font.h"
#include "core/renderer.h"
#include "imgui.h"
#include "platform/ANativeWindowCreator.h"
#include "platform/TouchHelperA.h"

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

} // namespace

int main() {
    using namespace android;

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

    aimgui::AppState st;
    st.permeate_record = false;

    WindowCtx ctx;
    if (!BuildWindow(&ctx, W, H, st.permeate_record)) {
        ImGui::DestroyContext();
        return 1;
    }
    st.renderer_name = ctx.renderer->Name();

    // readOnly = true: only read /dev/input/event* — do not create a uinput virtual device.
    Touch::Init({(float)W, (float)H}, true);
    Touch::setOrientation((int)info.orientation);

    using clock = std::chrono::steady_clock;
    auto last = clock::now();

    bool running = true;
    uint32_t cached_orientation = info.orientation;
    while (running) {
        auto now = clock::now();
        float dt = std::chrono::duration<float>(now - last).count();
        if (dt < 1.0f / 120.0f) {
            std::this_thread::sleep_for(std::chrono::microseconds(500));
            continue;
        }
        last = now;
        io.DeltaTime = dt > 0.f ? dt : 1.0f / 60.0f;

        info = ANativeWindowCreator::GetDisplayInfo();
        if (info.orientation != cached_orientation) {
            cached_orientation = info.orientation;
            Touch::setOrientation((int)info.orientation);
        }

        // Mirror display has to be processed only when our surface is visible
        // to recordings; when permeate_record is on, skip it.
        if (!st.permeate_record) {
            ANativeWindowCreator::ProcessMirrorDisplay();
        }

        ctx.renderer->NewFrame();
        ImGui::NewFrame();
        aimgui::AppFrame(&st, &running);
        ctx.renderer->EndFrame();

        // Handle anti-record toggle requested from UI: tear down and rebuild.
        if (st.request_permeate_toggle) {
            st.request_permeate_toggle = false;
            st.permeate_record = !st.permeate_record;

            DestroyWindow(&ctx);
            if (!BuildWindow(&ctx, W, H, st.permeate_record)) {
                running = false;
                break;
            }
            st.renderer_name = ctx.renderer->Name();
            // The font atlas survives in ImGui context; the new renderer
            // backend will lazily re-upload it on next frame.
        }
    }

    DestroyWindow(&ctx);
    ImGui::DestroyContext();
    return 0;
}
