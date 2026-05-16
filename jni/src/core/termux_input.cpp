#include "termux_input.h"

#include <atomic>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>
#include <sys/stat.h>
#include <thread>

namespace aimgui::termux_input {

namespace {

constexpr const char* kTermuxDialogPath =
    "/data/data/com.termux/files/usr/bin/termux-dialog";

// ─── State ───────────────────────────────────────────────────────────
std::atomic<bool> g_dialog_open{false};
std::atomic<bool> g_result_ready{false};

std::mutex   g_mtx;
std::string  g_result;          // protected by g_mtx
char*        g_dest_buf  = nullptr;
std::size_t  g_dest_size = 0;

std::thread  g_worker;          // detached on next launch / on Shutdown

// ─── Tiny JSON helper: pulls "text":"…" out of termux-dialog's reply ─
bool ParseTextJson(const std::string& json, std::string* out) {
    auto p = json.find("\"text\"");
    if (p == std::string::npos) return false;
    p = json.find(':', p);
    if (p == std::string::npos) return false;
    p = json.find('"', p);
    if (p == std::string::npos) return false;
    ++p;

    out->clear();
    while (p < json.size()) {
        char c = json[p];
        if (c == '"') return true;
        if (c == '\\' && p + 1 < json.size()) {
            char n = json[p + 1];
            switch (n) {
                case 'n':  out->push_back('\n'); break;
                case 't':  out->push_back('\t'); break;
                case 'r':  out->push_back('\r'); break;
                case '"':  out->push_back('"');  break;
                case '\\': out->push_back('\\'); break;
                case '/':  out->push_back('/');  break;
                default:   out->push_back(n);    break;
            }
            p += 2;
        } else {
            out->push_back(c);
            ++p;
        }
    }
    return false;
}

// Escape user content for use inside single-quoted shell args.
std::string EscapeShellSQ(const char* s) {
    std::string r = "'";
    if (s) {
        for (const char* p = s; *p; ++p) {
            if (*p == '\'') r += "'\\''";
            else            r.push_back(*p);
        }
    }
    r += "'";
    return r;
}

// ─── Worker: blocks in popen(), buffers stdout, parses, signals main ─
void RunDialogThread(std::string title, std::string hint) {
    char cmd[1024];
    std::snprintf(cmd, sizeof(cmd),
                  "%s text -t %s -i %s 2>/dev/null",
                  kTermuxDialogPath,
                  EscapeShellSQ(title.c_str()).c_str(),
                  EscapeShellSQ(hint.c_str()).c_str());

    FILE* fp = popen(cmd, "r");
    if (!fp) { g_dialog_open = false; return; }

    std::string output;
    char buf[256];
    while (std::fgets(buf, sizeof(buf), fp)) output += buf;
    pclose(fp);

    std::string text;
    ParseTextJson(output, &text);

    {
        std::lock_guard<std::mutex> lk(g_mtx);
        g_result = std::move(text);
    }
    g_result_ready.store(true, std::memory_order_release);
    g_dialog_open.store(false, std::memory_order_release);
}

} // namespace

// ─── Public API ──────────────────────────────────────────────────────
void Init() {}

void Shutdown() {
    if (g_worker.joinable()) g_worker.detach();
}

bool IsBusy() { return g_dialog_open.load(std::memory_order_acquire); }

bool IsAvailable() {
    static int cached = -1;
    if (cached == -1) {
        struct stat st;
        cached = (::stat(kTermuxDialogPath, &st) == 0) ? 1 : 0;
    }
    return cached == 1;
}

void Launch(char* dest_buf, std::size_t dest_buf_size,
            const char* title, const char* hint) {
    if (!IsAvailable())               return;
    if (g_dialog_open.exchange(true)) return;   // one dialog at a time

    g_dest_buf  = dest_buf;
    g_dest_size = dest_buf_size;
    g_result_ready.store(false, std::memory_order_release);

    if (g_worker.joinable()) g_worker.detach();
    g_worker = std::thread(RunDialogThread,
                           std::string(title ? title : "AImGui"),
                           std::string(hint  ? hint  : ""));
}

void Tick() {
    if (!g_result_ready.load(std::memory_order_acquire)) return;
    g_result_ready.store(false, std::memory_order_release);

    std::string text;
    {
        std::lock_guard<std::mutex> lk(g_mtx);
        text = std::move(g_result);
    }

    if (g_dest_buf && g_dest_size > 0) {
        std::size_t n = text.size();
        if (n > g_dest_size - 1) n = g_dest_size - 1;
        std::memcpy(g_dest_buf, text.data(), n);
        g_dest_buf[n] = '\0';
    }
    g_dest_buf  = nullptr;
    g_dest_size = 0;
}

} // namespace aimgui::termux_input
