// libaimgui_helper.so — loaded by the Java helper via System.load() to
// kick libbinder's worker thread pool from native code.
//
// We can't reach android.os.BinderInternal#joinThreadPool() from Java on
// some OEM ROMs (ColorOS/MIUI add a class-level blacklist on top of the
// AOSP hidden-API filter, which VMRuntime.setHiddenApiExemptions doesn't
// dislodge). Going through libbinder directly via dlsym sidesteps all
// the Java-side gating.
#include <android/log.h>
#include <dlfcn.h>
#include <jni.h>

#define LOG_TAG "AImGui_Boot"
#define LOGI(fmt, ...) __android_log_print(ANDROID_LOG_INFO,  LOG_TAG, fmt, ##__VA_ARGS__)
#define LOGW(fmt, ...) __android_log_print(ANDROID_LOG_WARN,  LOG_TAG, fmt, ##__VA_ARGS__)

// Itanium-mangled names of:
//   android::ProcessState* android::ProcessState::self()
//   void                   android::ProcessState::startThreadPool()
// `ProcessState::self()` actually returns sp<ProcessState> which on AArch64
// is a single-pointer struct → the raw ProcessState* lands in x0, which we
// then pass as `this` to startThreadPool().
static constexpr const char kSelfSym[]  = "_ZN7android12ProcessState4selfEv";
static constexpr const char kStartSym[] = "_ZN7android12ProcessState15startThreadPoolEv";

extern "C" JNIEXPORT jboolean JNICALL
Java_com_aimgui_BinderBoot_startThreadPool(JNIEnv*, jclass) {
    void* h = dlopen("libbinder.so", RTLD_NOW);
    if (!h) { LOGW("dlopen libbinder.so: %s", dlerror()); return JNI_FALSE; }

    auto self_fn  = reinterpret_cast<void* (*)()>(dlsym(h, kSelfSym));
    auto start_fn = reinterpret_cast<void  (*)(void*)>(dlsym(h, kStartSym));
    if (!self_fn || !start_fn) {
        LOGW("symbol lookup failed: self=%p start=%p", self_fn, start_fn);
        return JNI_FALSE;
    }

    void* ps = self_fn();
    if (!ps) { LOGW("ProcessState::self returned null"); return JNI_FALSE; }

    start_fn(ps);
    LOGI("ProcessState::startThreadPool invoked, ps=%p", ps);
    return JNI_TRUE;
}
