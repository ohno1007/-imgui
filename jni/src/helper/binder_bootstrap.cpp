// libaimgui_helper.so — loaded by the Java helper via System.load() to
// kick libbinder's worker thread pool from native code.
//
// On modern Android (especially OPlus/ColorOS) the Java helper runs in
// the "clns-1" classloader linker namespace whose permitted_paths are
// only /data:/mnt/expand — a plain dlopen("libbinder.so") gets denied
// because libbinder lives under /system/lib64 and transitively depends
// on /apex/com.android.runtime/lib64/bionic/libdl_android.so. We must
// either reuse the already-loaded copy via RTLD_DEFAULT or escape into
// a permissive namespace via android_dlopen_ext.
#include <android/dlext.h>
#include <android/log.h>
#include <cstdio>
#include <dlfcn.h>
#include <jni.h>

#define LOG_TAG "AImGui_Boot"
#define LOGI(fmt, ...) __android_log_print(ANDROID_LOG_INFO,  LOG_TAG, fmt, ##__VA_ARGS__)
#define LOGW(fmt, ...) __android_log_print(ANDROID_LOG_WARN,  LOG_TAG, fmt, ##__VA_ARGS__)

// Itanium-mangled C++ names. ProcessState::self() returns sp<ProcessState>,
// which on AArch64 is a one-pointer struct returned in x0, so the raw
// ProcessState* is the call's return value and we pass it as `this` to
// startThreadPool().
static constexpr const char kSelfSym[]  = "_ZN7android12ProcessState4selfEv";
static constexpr const char kStartSym[] = "_ZN7android12ProcessState15startThreadPoolEv";

static bool invoke_pool_with(void* sym_src, const char* via) {
    auto self_fn  = reinterpret_cast<void* (*)()>     (dlsym(sym_src, kSelfSym));
    auto start_fn = reinterpret_cast<void  (*)(void*)>(dlsym(sym_src, kStartSym));
    if (!self_fn || !start_fn) {
        LOGW("%s: dlsym self=%p start=%p", via, self_fn, start_fn);
        return false;
    }
    void* ps = self_fn();
    if (!ps) { LOGW("%s: ProcessState::self() returned null", via); return false; }
    start_fn(ps);
    LOGI("%s: ProcessState::startThreadPool invoked (ps=%p)", via, ps);
    return true;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_aimgui_BinderBoot_startThreadPool(JNIEnv*, jclass) {
    // Path 1 — libbinder may already be in the process's global symbol
    // table (app_process loads it at boot). RTLD_DEFAULT is namespace-
    // local but cheap to try first.
    if (invoke_pool_with(RTLD_DEFAULT, "RTLD_DEFAULT")) return JNI_TRUE;

    // Path 2 — escape clns-1 by dlopening libbinder into an exported
    // namespace that can see /system + /apex. "default" is the bootstrap
    // namespace, "system" is the same on most ROMs.
    using get_ns_t = struct android_namespace_t* (*)(const char*);
    auto get_ns = reinterpret_cast<get_ns_t>(
            dlsym(RTLD_DEFAULT, "android_get_exported_namespace"));
    if (get_ns) {
        const char* names[] = { "default", "system", "art", nullptr };
        for (const char** np = names; *np; ++np) {
            struct android_namespace_t* ns = get_ns(*np);
            if (!ns) { LOGW("namespace '%s' not exported", *np); continue; }
            android_dlextinfo info{};
            info.flags = ANDROID_DLEXT_USE_NAMESPACE;
            info.library_namespace = ns;
            void* h = android_dlopen_ext("libbinder.so", RTLD_NOW, &info);
            if (!h) { LOGW("ns=%s: %s", *np, dlerror()); continue; }
            char tag[32];
            std::snprintf(tag, sizeof(tag), "ns=%s", *np);
            if (invoke_pool_with(h, tag)) return JNI_TRUE;
        }
    } else {
        LOGW("android_get_exported_namespace unavailable");
    }

    // Path 3 — plain dlopen, last-ditch. Usually denied by clns-1 but
    // some forks loosen the namespace policy.
    if (void* h = dlopen("libbinder.so", RTLD_NOW)) {
        if (invoke_pool_with(h, "plain-dlopen")) return JNI_TRUE;
    } else {
        LOGW("plain dlopen libbinder.so: %s", dlerror());
    }

    return JNI_FALSE;
}
