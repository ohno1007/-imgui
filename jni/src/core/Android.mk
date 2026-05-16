LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE := aimgui_core

LOCAL_CPPFLAGS := -std=c++17 -fexceptions -DVK_USE_PLATFORM_ANDROID_KHR -DIMGUI_IMPL_VULKAN_NO_PROTOTYPES

LOCAL_C_INCLUDES        := $(LOCAL_PATH)
LOCAL_EXPORT_C_INCLUDES := $(LOCAL_PATH)

LOCAL_STATIC_LIBRARIES := imgui

LOCAL_SRC_FILES := \
    renderer_factory.cpp \
    renderer_gl.cpp \
    renderer_vk.cpp \
    vulkan_wrapper.cpp \
    font.cpp \
    keyboard_input.cpp \
    termux_input.cpp

include $(BUILD_STATIC_LIBRARY)
