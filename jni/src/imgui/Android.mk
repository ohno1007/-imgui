LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE := imgui

LOCAL_CPPFLAGS := -std=c++17 -fexceptions -DVK_USE_PLATFORM_ANDROID_KHR -DIMGUI_IMPL_VULKAN_NO_PROTOTYPES

LOCAL_C_INCLUDES := \
    $(LOCAL_PATH) \
    $(LOCAL_PATH)/backends

LOCAL_EXPORT_C_INCLUDES := \
    $(LOCAL_PATH) \
    $(LOCAL_PATH)/backends

LOCAL_SRC_FILES := \
    imgui.cpp \
    imgui_draw.cpp \
    imgui_tables.cpp \
    imgui_widgets.cpp \
    backends/imgui_impl_opengl3.cpp \
    backends/imgui_impl_vulkan.cpp

include $(BUILD_STATIC_LIBRARY)
