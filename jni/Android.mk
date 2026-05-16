LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE := AImGui

LOCAL_CFLAGS   := -std=c17 -Wall
LOCAL_CPPFLAGS := -std=c++17 -fexceptions -Wall

LOCAL_C_INCLUDES := \
    $(LOCAL_PATH)/src \
    $(LOCAL_PATH)/src/imgui \
    $(LOCAL_PATH)/src/imgui/backends \
    $(LOCAL_PATH)/src/platform \
    $(LOCAL_PATH)/src/core

LOCAL_SRC_FILES := \
    src/main.cpp \
    src/app.cpp \
    src/core/renderer.cpp \
    src/platform/TouchHelperA.cpp \
    src/imgui/imgui.cpp \
    src/imgui/imgui_draw.cpp \
    src/imgui/imgui_tables.cpp \
    src/imgui/imgui_widgets.cpp \
    src/imgui/backends/imgui_impl_opengl3.cpp

LOCAL_LDLIBS := -llog -landroid -lEGL -lGLESv3

include $(BUILD_EXECUTABLE)
