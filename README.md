# AImGui

A minimal, elegant **Dear ImGui** framework that builds to a single Android
ARM64 ELF executable — no JNI, no APK, no Activity. Runs as a native binary on
top of SurfaceFlinger.

- ImGui **v1.92.6**
- **Vulkan + OpenGL ES 3** backends with automatic VK→GL fallback
- **System CJK font** auto-detected (`NotoSansCJK`, `DroidSansFallback`, …)
- Layered into three independently-updatable static libraries
- No `imgui_demo`, no debug tools, no FreeType, no FontAwesome
- Aggressive size optimization (`-Os`, LTO, `--gc-sections`, `--icf=all`, strip)

Stripped ELF for `arm64-v8a`: **~660 KB**.

## Layout

```
jni/
├── Android.mk                       umbrella; pulls in submodules
├── Application.mk
└── src/
    ├── main.cpp                     entry point + main loop
    ├── app.cpp, app.h               your UI lives here
    │
    ├── imgui/                       ── libimgui.a (vendored, swap to update ImGui) ──
    │   ├── Android.mk
    │   ├── imconfig.h, imgui.h, imgui_internal.h
    │   ├── imgui.cpp, imgui_draw.cpp, imgui_tables.cpp, imgui_widgets.cpp
    │   ├── imstb_rectpack.h, imstb_textedit.h, imstb_truetype.h
    │   └── backends/
    │       ├── imgui_impl_opengl3.{h,cpp}, imgui_impl_opengl3_loader.h
    │       └── imgui_impl_vulkan.{h,cpp}
    │
    ├── platform/                    ── libaimgui_platform.a (vendored glue) ──
    │   ├── Android.mk
    │   ├── ANativeWindowCreator.h   SurfaceFlinger window (MIT)
    │   └── TouchHelperA.{h,cpp}, Utils.h, VectorStruct.h, spinlock.h
    │
    └── core/                        ── libaimgui_core.a (project-owned) ──
        ├── Android.mk
        ├── renderer.h               IRenderer interface + MakeRenderer()
        ├── renderer_factory.cpp     VK→GL fallback
        ├── renderer_gl.cpp          EGL + OpenGL ES 3
        ├── renderer_vk.cpp          Vulkan (Android Surface)
        ├── vulkan_wrapper.{h,cpp}   dynamic loader (no -lvulkan needed)
        └── font.{h,cpp}             system CJK font loader
```

Each subdirectory is a self-contained NDK module. To upgrade ImGui, drop in
the new source files under `jni/src/imgui/` and rebuild — the rest of the
project doesn't need to change.

## Backend selection

By default `aimgui::MakeRenderer(Backend::Auto)` tries Vulkan first and falls
back to OpenGL ES 3. Force either backend explicitly:

```cpp
auto r = aimgui::MakeRenderer(window, W, H, aimgui::Backend::OpenGL); // or Vulkan
```

## CJK font

`aimgui::LoadDefaultAndSystemCJKFont(size_px)` adds the bundled ProggyClean
default and merges the system font found at:

```
/system/fonts/NotoSansCJK-Regular.ttc
/system/fonts/NotoSerifCJK-Regular.ttc
/system/fonts/NotoSansCJKsc-Regular.otf
/system/fonts/DroidSansFallbackFull.ttf
/system/fonts/DroidSansFallback.ttf
/system/fonts/NotoSansSC-Regular.otf
```

Glyphs are rasterized lazily, so the atlas stays small even with the full
0x4E00–0x9FFF CJK Unified Ideographs range enabled.

## Build

```bash
export ANDROID_NDK_HOME=/path/to/android-ndk-r27c   # r25+
./build.sh
# → libs/arm64-v8a/AImGui
```

## Run

The binary creates a SurfaceFlinger overlay and reads `/dev/input/*`, so it
needs elevated privileges (root, or a shell with the right capabilities):

```bash
adb push libs/arm64-v8a/AImGui /data/local/tmp/AImGui
adb shell chmod +x /data/local/tmp/AImGui
adb shell su -c /data/local/tmp/AImGui
```

A draggable, Chinese-capable ImGui window appears over the current display.

## Customize

All UI lives in `jni/src/app.cpp`:

```cpp
void aimgui::AppFrame(bool* keep_running) {
    ImGui::Begin(u8"你好");
    ImGui::Text(u8"渲染后端：%s", g_RendererName);
    ImGui::End();
}
```

## Credits

- [Dear ImGui](https://github.com/ocornut/imgui) — Omar Cornut
- [AndroidSurfaceImgui-Enhanced](https://github.com/AFan4724/AndroidSurfaceImgui-Enhanced) — window + touch glue (MIT)
- Vulkan wrapper from Google's NDK samples (Apache-2.0)
