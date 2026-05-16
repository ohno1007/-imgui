# AImGui

A minimal, elegant **Dear ImGui** framework that compiles to a single Android
ARM64 ELF executable — no JNI, no APK, no Activity. Runs as a native binary on
top of SurfaceFlinger.

- ImGui **v1.92.6**
- OpenGL ES 3 backend only (no Vulkan)
- No FreeType, no FontAwesome, no embedded image / Chinese font blobs
- No `imgui_demo`, no debug tools
- Aggressive size optimization (`-Os`, LTO, `--gc-sections`, `--icf=all`, strip)

A complete build produces a **~630 KB** stripped ELF for `arm64-v8a`.

## Layout

```
jni/
├── Android.mk
├── Application.mk
└── src/
    ├── main.cpp                     entry point + main loop
    ├── app.cpp / app.h              your UI lives here
    ├── core/
    │   └── renderer.{h,cpp}         EGL + GLES3 setup / present
    ├── imgui/                       vendored ImGui core (slim imconfig.h)
    │   └── backends/                imgui_impl_opengl3 only
    └── platform/                    Android glue
        ├── ANativeWindowCreator.h   SurfaceFlinger window (vendored, MIT)
        ├── TouchHelperA.{h,cpp}     /dev/input touch helper (vendored)
        └── Utils.h, VectorStruct.h, spinlock.h
```

## Build

```bash
export ANDROID_NDK_HOME=/path/to/android-ndk-r27c
./build.sh
# → libs/arm64-v8a/AImGui
```

Or directly:

```bash
$ANDROID_NDK_HOME/ndk-build NDK_PROJECT_PATH=. \
    APP_BUILD_SCRIPT=jni/Android.mk \
    NDK_APPLICATION_MK=jni/Application.mk
```

## Run

The binary creates a SurfaceFlinger overlay and reads `/dev/input/*`, which
requires elevated privileges on Android. Push and run via `adb` on a rooted
device:

```bash
adb push libs/arm64-v8a/AImGui /data/local/tmp/AImGui
adb shell chmod +x /data/local/tmp/AImGui
adb shell su -c /data/local/tmp/AImGui
```

A draggable ImGui window will appear over the current display.

## Customize

All UI is in `jni/src/app.cpp`:

```cpp
void aimgui::AppFrame(bool* keep_running) {
    ImGui::Begin("Hello");
    ImGui::Text("Hi from AImGui");
    ImGui::End();
}
```

## Credits

- [Dear ImGui](https://github.com/ocornut/imgui) — Omar Cornut
- [AndroidSurfaceImgui-Enhanced](https://github.com/AFan4724/AndroidSurfaceImgui-Enhanced) — window + touch glue (MIT)
