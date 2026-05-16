JNI_ROOT := $(call my-dir)

# Pull in each static-library subproject.
include $(JNI_ROOT)/src/imgui/Android.mk
include $(JNI_ROOT)/src/platform/Android.mk
include $(JNI_ROOT)/src/core/Android.mk

# Final executable: main + ui (compiled directly into the binary, not a lib).
include $(CLEAR_VARS)
LOCAL_PATH := $(JNI_ROOT)
LOCAL_MODULE := AImGui

LOCAL_CPPFLAGS := -std=c++17 -fexceptions

LOCAL_C_INCLUDES := \
    $(LOCAL_PATH)/include \
    $(LOCAL_PATH)/src

LOCAL_SRC_FILES := \
    src/main.cpp \
    src/ui/ui.cpp

LOCAL_STATIC_LIBRARIES := aimgui_core aimgui_platform imgui

LOCAL_LDLIBS := -llog -landroid -lEGL -lGLESv3

include $(BUILD_EXECUTABLE)
