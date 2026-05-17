LOCAL_PATH := $(call my-dir)

# Prebuilt Dear ImGui static library. Sources live outside this repo and are
# rebuilt by the imgui-rebuild target (see scripts/build_imgui.sh); the
# resulting libimgui.a + public headers are committed under prebuilt/ and
# jni/include/imgui/ respectively.
include $(CLEAR_VARS)
LOCAL_MODULE := imgui
LOCAL_SRC_FILES := ../prebuilt/$(TARGET_ARCH_ABI)/libimgui.a
LOCAL_EXPORT_C_INCLUDES := \
    $(LOCAL_PATH)/include/imgui \
    $(LOCAL_PATH)/include/imgui/backends
include $(PREBUILT_STATIC_LIBRARY)
