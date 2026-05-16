// AImGui: a minimal Dear ImGui Android ARM64 ELF.
#include "app.h"
#include "core/font.h"
#include "core/renderer.h"
#include "imgui.h"
#include "platform/ANativeWindowCreator.h"
#include "platform/TouchHelperA.h"

#include <android/log.h>
#include <chrono>
#include <thread>

#define LOG_TAG "AImGui"
#define LOGI(fmt, ...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, fmt, ##__VA_ARGS__)
#define LOGE(fmt, ...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, fmt, ##__VA_ARGS__)

namespace {

struct WindowCtx {
    ANativeWindow* window = nullptr;
    std::unique_ptr<aimgui::IRenderer> renderer;
};

bool BuildWindow(WindowCtx* ctx, int W, int H, bool permeate_record) {
    LOGI("BuildWindow: creating SurfaceFlinger surface (permeate=%d)", permeate_record);
    ctx->window = android::ANativeWindowCreator::Create("AImGui", W, W, permeate_record);
    if (!ctx->window) {
        LOGE("ANativeWindowCreator::Create failed");
        return false;
    }
    LOGI("BuildWindow: creating renderer");
    ctx->renderer = aimgui::MakeRenderer(ctx->window, W, W, aimgui::Backend::Auto);
    if (!ctx->renderer) {
        LOGE("MakeRenderer failed");
        android::ANativeWindowCreator::Destroy(ctx->window);
        ctx->window = nullptr;
        return false;
    }
    LOGI("BuildWindow: renderer = %s", ctx->renderer->Name());
    return true;
}

void DestroyWindow(WindowCtx* ctx) {
    if (ctx->renderer) { ctx->renderer->Shutdown(); ctx->renderer.reset(); }
    if (ctx->window)   { android::ANativeWindowCreator::Destroy(ctx->window); ctx->window = nullptr; }
}

} // namespace

int main() {
    using namespace android;

    LOGI("== AImGui starting ==");
    auto info = ANativeWindowCreator::GetDisplayInfo();
    const int W = info.width  > info.height ? info.width  : info.height;
    const int H = info.width  > info.height ? info.height : info.width;
    LOGI("display: %dx%d  orientation=%u", info.width, info.height, info.orientation);

    LOGI("ImGui::CreateContext");
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.LogFilename = nullptr;
    io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;

    ImGui::StyleColorsDark();
    ImGui::GetStyle().ScaleAllSizes(2.5f);

    LOGI("LoadDefaultAndSystemCJKFont");
    aimgui::LoadDefaultAndSystemCJKFont(22.0f);

    aimgui::AppState st;
    st.permeate_record = false;

    WindowCtx ctx;
    if (!BuildWindow(&ctx, W, H, st.permeate_record)) {
        LOGE("BuildWindow failed at startup, exiting");
        ImGui::DestroyContext();
        return 1;
    }
    st.renderer_name = ctx.renderer->Name();

    LOGI("Touch::Init (readOnly)");
    Touch::Init({(float)W, (float)H}, true);   // readOnly: no /dev/uinput, no virtual device
    Touch::setOrientation((int)info.orientation);

    LOGI("entering main loop");

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

        if (!st.permeate_record) {
            ANativeWindowCreator::ProcessMirrorDisplay();
        }

        ctx.renderer->NewFrame();
        ImGui::NewFrame();
        aimgui::AppFrame(&st, &running);
        ctx.renderer->EndFrame();

        if (st.request_permeate_toggle) {
            st.request_permeate_toggle = false;
            st.permeate_record = !st.permeate_record;
            LOGI("toggling permeate_record -> %d", st.permeate_record);

            DestroyWindow(&ctx);
            if (!BuildWindow(&ctx, W, H, st.permeate_record)) {
                running = false;
                break;
            }
            st.renderer_name = ctx.renderer->Name();
        }
    }

    LOGI("shutdown: stopping touch threads");
    Touch::Close();
    LOGI("shutdown: destroying renderer + window");
    DestroyWindow(&ctx);
    LOGI("shutdown: destroying ImGui context");
    ImGui::DestroyContext();
    LOGI("== AImGui exit ==");
    return 0;
}
