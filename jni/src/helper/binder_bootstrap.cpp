// libaimgui_helper.so — loaded by the Java helper to kick libbinder's
// thread pool from native code, sidestepping every Java-side gate.
#include <android/dlext.h>
#include <android/log.h>
#include <cstdio>
#include <cstring>
#include <dlfcn.h>
#include <jni.h>
#include <link.h>

#define LOG_TAG "AImGui_IME"
#define LOGI(fmt, ...) __android_log_print(ANDROID_LOG_INFO,  LOG_TAG, "[boot] " fmt, ##__VA_ARGS__)
#define LOGW(fmt, ...) __android_log_print(ANDROID_LOG_WARN,  LOG_TAG, "[boot] " fmt, ##__VA_ARGS__)

// ProcessState::self() returns sp<ProcessState>; on AArch64 the underlying
// pointer is the return value, so we treat it as a `void*` and pass it as
// `this` to startThreadPool().
static constexpr const char kSelfSym[]  = "_ZN7android12ProcessState4selfEv";
static constexpr const char kStartSym[] = "_ZN7android12ProcessState15startThreadPoolEv";

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

// dlsym across (RTLD_DEFAULT, libdl_handle) for each candidate name.
// RTLD_DEFAULT is literally ((void*)0) on bionic, so the earlier
// "for (handles[i]; ...)" loop never iterated — explicit here.
static void* find_sym(void* libdl, const char* const* names, const char* what) {
    for (const char* const* n = names; *n; ++n) {
        if (void* p = dlsym(RTLD_DEFAULT, *n)) {
            LOGI("resolved %s as %s via RTLD_DEFAULT", what, *n);
            return p;
        }
        if (libdl) {
            if (void* p = dlsym(libdl, *n)) {
                LOGI("resolved %s as %s via libdl handle", what, *n);
                return p;
            }
        }
    }
    LOGW("could not resolve %s", what);
    return nullptr;
}

static bool invoke_pool(void* sym_src, const char* via) {
    auto self_fn  = reinterpret_cast<void* (*)()>     (dlsym(sym_src, kSelfSym));
    auto start_fn = reinterpret_cast<void  (*)(void*)>(dlsym(sym_src, kStartSym));
    if (!self_fn || !start_fn) {
        const char* err = dlerror();
        LOGW("%s: ProcessState::self=%p ProcessState::startThreadPool=%p (%s)",
             via, self_fn, start_fn, err ? err : "no err");
        return false;
    }
    void* ps = self_fn();
    if (!ps) { LOGW("%s: ProcessState::self() returned null", via); return false; }
    start_fn(ps);
    LOGI("%s: ProcessState::startThreadPool invoked (ps=%p)", via, ps);
    return true;
}

// Diagnostic: list every shared object currently loaded in this process.
static int phdr_log_cb(struct dl_phdr_info* info, size_t, void*) {
    const char* n = (info->dlpi_name && *info->dlpi_name) ? info->dlpi_name : "(main)";
    LOGI("loaded: '%s' base=%p", n, reinterpret_cast<void*>(info->dlpi_addr));
    return 0;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_aimgui_BinderBoot_startThreadPool(JNIEnv*, jclass) {
    // Path 1 — symbols may already be in the process global view (e.g. if
    // Java has triggered any binder op already).
    if (invoke_pool(RTLD_DEFAULT, "RTLD_DEFAULT")) return JNI_TRUE;

    // Loaded-library diagnostics.
    LOGI("--- dl_iterate_phdr ---");
    dl_iterate_phdr(phdr_log_cb, nullptr);
    LOGI("--- end iterate ---");

    // libdl handle for finding namespace APIs (RTLD_DEFAULT may filter
    // them out in clns-1).
    void* libdl = dlopen("libdl.so", RTLD_NOW);
    if (!libdl) {
        const char* e = dlerror();
        LOGW("dlopen libdl.so: %s", e ? e : "?");
    } else {
        LOGI("libdl handle = %p", libdl);
    }

    auto get_ns     = reinterpret_cast<get_ns_t>    (find_sym(libdl, kGetNsNames,    "android_get_exported_namespace"));
    auto dlopen_ext = reinterpret_cast<dlopen_ext_t>(find_sym(libdl, kDlopenExtNames, "android_dlopen_ext"));

    // Path 2 — namespace escape via android_dlopen_ext.
    if (get_ns && dlopen_ext) {
        const char* nss[] = {
            "default", "system", "art", "com_android_art", "runtime",
            "neuralnetworks", nullptr };
        for (const char** np = nss; *np; ++np) {
            struct android_namespace_t* ns = get_ns(*np);
            if (!ns) { LOGW("namespace '%s' not exported", *np); continue; }
            android_dlextinfo info{};
            info.flags = ANDROID_DLEXT_USE_NAMESPACE;
            info.library_namespace = ns;
            void* h = dlopen_ext("libbinder.so", RTLD_NOW, &info);
            if (!h) { LOGW("ns=%s dlopen failed: %s", *np, dlerror()); continue; }
            char tag[48];
            std::snprintf(tag, sizeof(tag), "ns=%s", *np);
            if (invoke_pool(h, tag)) return JNI_TRUE;
        }
    } else {
        LOGW("namespace API unavailable; falling through");
    }

    // Path 3 — plain dlopen (clns-1 almost certainly denies).
    if (void* h = dlopen("libbinder.so", RTLD_NOW)) {
        if (invoke_pool(h, "plain-dlopen")) return JNI_TRUE;
    } else {
        LOGW("plain dlopen libbinder.so: %s", dlerror());
    }

    LOGW("all paths exhausted; ProcessState::startThreadPool was NOT invoked");
    return JNI_FALSE;
}
