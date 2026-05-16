package com.aimgui;

/** JNI hook to libaimgui_helper.so. The native side dlsym's
 *  ProcessState::self()/startThreadPool() out of libbinder.so. */
public final class BinderBoot {
    public static native boolean startThreadPool();
    private BinderBoot() {}
}
