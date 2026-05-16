// libaimgui_helper.so — start libbinder's worker thread pool from native
// code, even when the linker namespace forbids us from dlopen()ing it.
//
// On modern Android the Java helper runs in linker namespace "clns-1"
// whose permitted_paths are /data:/mnt/expand only. libbinder.so lives
// at /system/lib64/libbinder.so, in a separate namespace ("default"), so
//   dlopen("libbinder.so")                 → DENIED (namespace path)
//   dlsym(RTLD_DEFAULT, "ProcessState…")   → null  (different namespace)
//   android_dlopen_ext with ns="default"   → libdl's extension symbols
//                                             are also filtered out
//
// The library IS already loaded into the process (the JVM links it for
// Binder support); dl_iterate_phdr exposes its base address. So instead
// of going through the dynamic linker, we walk libbinder's PT_DYNAMIC →
// SYMTAB / GNU_HASH directly and resolve ProcessState::self /
// startThreadPool ourselves. No linker machinery, no namespace gate.
#include <android/log.h>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <dlfcn.h>
#include <elf.h>
#include <jni.h>
#include <link.h>

#define LOG_TAG "AImGui_IME"
#define LOGI(fmt, ...) __android_log_print(ANDROID_LOG_INFO,  LOG_TAG, "[boot] " fmt, ##__VA_ARGS__)
#define LOGW(fmt, ...) __android_log_print(ANDROID_LOG_WARN,  LOG_TAG, "[boot] " fmt, ##__VA_ARGS__)

static constexpr const char kSelfSym[]  = "_ZN7android12ProcessState4selfEv";
static constexpr const char kStartSym[] = "_ZN7android12ProcessState15startThreadPoolEv";

#if __LP64__
using ElfSym  = Elf64_Sym;
using ElfPhdr = Elf64_Phdr;
using ElfDyn  = Elf64_Dyn;
using ElfWord = uint64_t;
#else
using ElfSym  = Elf32_Sym;
using ElfPhdr = Elf32_Phdr;
using ElfDyn  = Elf32_Dyn;
using ElfWord = uint32_t;
#endif

struct ParsedLib {
    uintptr_t        base     = 0;
    const ElfSym*    symtab   = nullptr;
    const char*      strtab   = nullptr;
    const uint32_t*  gnu_hash = nullptr;
};

static bool parse_lib(uintptr_t base, const ElfPhdr* phdrs, int phnum, ParsedLib* out) {
    out->base = base;
    const ElfDyn* dyn = nullptr;
    for (int i = 0; i < phnum; ++i) {
        if (phdrs[i].p_type == PT_DYNAMIC) {
            dyn = reinterpret_cast<const ElfDyn*>(base + phdrs[i].p_vaddr);
            break;
        }
    }
    if (!dyn) return false;
    for (const ElfDyn* d = dyn; d->d_tag != DT_NULL; ++d) {
        switch (d->d_tag) {
            case DT_SYMTAB:    out->symtab   = reinterpret_cast<const ElfSym*>  (base + d->d_un.d_ptr); break;
            case DT_STRTAB:    out->strtab   = reinterpret_cast<const char*>   (base + d->d_un.d_ptr); break;
            case DT_GNU_HASH:  out->gnu_hash = reinterpret_cast<const uint32_t*>(base + d->d_un.d_ptr); break;
            default: break;
        }
    }
    return out->symtab && out->strtab && out->gnu_hash;
}

static uint32_t gnu_hash(const char* s) {
    uint32_t h = 5381;
    while (*s) h = h * 33 + (uint8_t)*s++;
    return h;
}

static void* lookup_symbol(const ParsedLib& lib, const char* name) {
    if (!lib.gnu_hash) return nullptr;
    const uint32_t nbuckets    = lib.gnu_hash[0];
    const uint32_t symbias     = lib.gnu_hash[1];
    const uint32_t bloom_size  = lib.gnu_hash[2];
    const uint32_t bloom_shift = lib.gnu_hash[3];
    const ElfWord*  bloom    = reinterpret_cast<const ElfWord*>(lib.gnu_hash + 4);
    const uint32_t* buckets  = reinterpret_cast<const uint32_t*>(bloom + bloom_size);
    const uint32_t* chain    = buckets + nbuckets;

    const uint32_t h = gnu_hash(name);

    const ElfWord bw = bloom[(h / (sizeof(ElfWord) * 8)) % bloom_size];
    const ElfWord mask = (ElfWord{1} << (h % (sizeof(ElfWord) * 8)))
                       | (ElfWord{1} << ((h >> bloom_shift) % (sizeof(ElfWord) * 8)));
    if ((bw & mask) != mask) return nullptr;

    uint32_t idx = buckets[h % nbuckets];
    if (idx < symbias) return nullptr;

    while (true) {
        const uint32_t cval = chain[idx - symbias];
        if (((cval ^ h) >> 1) == 0) {
            const ElfSym& sym = lib.symtab[idx];
            if (std::strcmp(lib.strtab + sym.st_name, name) == 0 && sym.st_value != 0) {
                return reinterpret_cast<void*>(lib.base + sym.st_value);
            }
        }
        if (cval & 1) break;
        ++idx;
    }
    return nullptr;
}

struct Hunt {
    const char* lib_substr;
    void*       self_fn;
    void*       start_fn;
    const char* found_path;
};

static int hunt_cb(struct dl_phdr_info* info, size_t, void* data) {
    auto* h = static_cast<Hunt*>(data);
    if (!info->dlpi_name || !std::strstr(info->dlpi_name, h->lib_substr)) return 0;
    ParsedLib lib;
    if (!parse_lib(info->dlpi_addr, info->dlpi_phdr, info->dlpi_phnum, &lib)) {
        LOGW("found %s but couldn't parse ELF", info->dlpi_name);
        return 0;
    }
    h->self_fn  = lookup_symbol(lib, kSelfSym);
    h->start_fn = lookup_symbol(lib, kStartSym);
    if (h->self_fn && h->start_fn) {
        h->found_path = info->dlpi_name;
        return 1;
    }
    LOGW("found %s but symbols missing: self=%p start=%p",
         info->dlpi_name, h->self_fn, h->start_fn);
    return 0;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_aimgui_BinderBoot_startThreadPool(JNIEnv*, jclass) {
    Hunt h { "libbinder.so", nullptr, nullptr, nullptr };
    dl_iterate_phdr(hunt_cb, &h);

    if (!h.self_fn || !h.start_fn) {
        LOGW("could not resolve ProcessState symbols inside any loaded libbinder");
        return JNI_FALSE;
    }

    auto self_fn  = reinterpret_cast<void* (*)()>     (h.self_fn);
    auto start_fn = reinterpret_cast<void  (*)(void*)>(h.start_fn);
    void* ps = self_fn();
    if (!ps) { LOGW("ProcessState::self() returned null"); return JNI_FALSE; }
    start_fn(ps);
    LOGI("ProcessState::startThreadPool() invoked via ELF lookup in %s (ps=%p)",
         h.found_path, ps);
    return JNI_TRUE;
}
