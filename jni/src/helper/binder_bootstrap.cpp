// libaimgui_helper.so — loaded by the Java helper to kick libbinder's
// thread pool from native code, sidestepping every Java-side gate.
//
// app_process puts the Java helper's loaded .so into the "clns-1"
// classloader namespace, whose permitted_paths are "/data:/mnt/expand"
// only. libbinder.so (under /system/lib64, transitively pulling in
// /apex/com.android.runtime/lib64/bionic/libdl_android.so) is therefore
// invisible to a plain dlopen() from here. We have to either reuse the
// already-loaded copy via RTLD_DEFAULT or escape the namespace via
// android_dlopen_ext into "default" / "system" / "art".
#include <android/dlext.h>
#include <android/log.h>
#include <cstdio>
#include <cstring>
#include <dlfcn.h>
#include <jni.h>

// Use the same tag as the Java helper so user's existing
//   logcat -s AImGui_IME:*
// filter picks these messages up.
#define LOG_TAG "AImGui_IME"
#define LOGI(fmt, ...) __android_log_print(ANDROID_LOG_INFO,  LOG_TAG, "[boot] " fmt, ##__VA_ARGS__)
#define LOGW(fmt, ...) __android_log_print(ANDROID_LOG_WARN,  LOG_TAG, "[boot] " fmt, ##__VA_ARGS__)

static constexpr const char kSelfSym[]  = "_ZN7android12ProcessState4selfEv";
static constexpr const char kStartSym[] = "_ZN7android12ProcessState15startThreadPoolEv";

// Try the public name first, then the linker-internal alias.
static constexpr const char* kGetNsNames[] = {
    "android_get_exported_namespace",
    "__loader_android_get_exported_namespace",
    nullptr,
};
static constexpr const char* kDlopenExtNames[] = {
    "android_dlopen_ext",
    "__loader_android_dlopen_ext",
    nullptr,
};

using get_ns_t      = struct android_namespace_t* (*)(const char*);
using dlopen_ext_t  = void* (*)(const char*, int, const android_dlextinfo*);

static void* resolve(void** handles, const char* const* names, const char* what) {
    for (const char* const* n = names; *n; ++n) {
        for (int i = 0; handles[i]; ++i) {
            void* p = dlsym(handles[i], *n);
            if (p) { LOGI("resolved %s = %s @ handle[%d]", what, *n, i); return p; }
        }
    }
    LOGW("could not resolve %s", what);
    return nullptr;
}

static bool invoke_pool(void* sym_src, const char* via) {
    auto self_fn  = reinterpret_cast<void* (*)()>     (dlsym(sym_src, kSelfSym));
    auto start_fn = reinterpret_cast<void  (*)(void*)>(dlsym(sym_src, kStartSym));
    if (!self_fn || !start_fn) {
        LOGW("%s: dlsym ProcessState::self=%p ProcessState::startThreadPool=%p (%s)",
             via, self_fn, start_fn, dlerror() ? dlerror() : "no err");
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
    // Open candidate library handles up front. RTLD_DEFAULT first (cheap;
    // works if libbinder is already in our namespace view), then explicit
    // libdl.so so we can find linker-internal entry points even when
    // RTLD_DEFAULT's view is restricted.
    void* libdl = dlopen("libdl.so", RTLD_NOW);
    void* handles[] = { RTLD_DEFAULT, libdl, nullptr };

    // Path 1 — symbols already in process global view.
    if (invoke_pool(RTLD_DEFAULT, "RTLD_DEFAULT")) return JNI_TRUE;

    // Path 2 — android_dlopen_ext into an exported namespace that can
    // actually see /system + /apex (default / system / art).
    auto get_ns = reinterpret_cast<get_ns_t>(
            resolve(handles, kGetNsNames, "android_get_exported_namespace"));
    auto dlopen_ext = reinterpret_cast<dlopen_ext_t>(
            resolve(handles, kDlopenExtNames, "android_dlopen_ext"));

    if (get_ns && dlopen_ext) {
        const char* nss[] = { "default", "system", "art",
                              "com_android_art", "runtime", nullptr };
        for (const char** np = nss; *np; ++np) {
            struct android_namespace_t* ns = get_ns(*np);
            if (!ns) { LOGW("namespace '%s' not exported", *np); continue; }
            android_dlextinfo info{};
            info.flags = ANDROID_DLEXT_USE_NAMESPACE;
            info.library_namespace = ns;
            void* h = dlopen_ext("libbinder.so", RTLD_NOW, &info);
            if (!h) { LOGW("ns=%s: %s", *np, dlerror()); continue; }
            char tag[48];
            std::snprintf(tag, sizeof(tag), "ns=%s", *np);
            if (invoke_pool(h, tag)) return JNI_TRUE;
        }
    } else {
        LOGW("namespace API unavailable; falling through");
    }

    // Path 3 — plain dlopen (usually denied by clns-1).
    if (void* h = dlopen("libbinder.so", RTLD_NOW)) {
        if (invoke_pool(h, "plain-dlopen")) return JNI_TRUE;
    } else {
        LOGW("plain dlopen libbinder.so: %s", dlerror());
    }

    // Path 4 — last-ditch: explicit absolute path, with caller-addr
    // workaround would go here, but skip the heavyweight tricks for now.
    LOGW("all paths exhausted; ProcessState::startThreadPool was NOT invoked");
    return JNI_FALSE;
}
