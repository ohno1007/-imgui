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

# Small JNI helper loaded by the Java IME helper to start libbinder's
# thread pool natively — avoids the Java-side hidden-API gate that
# blocks android.os.BinderInternal on some OEM ROMs.
include $(CLEAR_VARS)
LOCAL_PATH := $(JNI_ROOT)
LOCAL_MODULE     := aimgui_helper
LOCAL_CPPFLAGS   := -std=c++17 -fexceptions
LOCAL_SRC_FILES  := src/helper/binder_bootstrap.cpp
LOCAL_LDLIBS     := -llog -ldl
include $(BUILD_SHARED_LIBRARY)
