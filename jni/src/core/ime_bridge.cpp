#include "ime_bridge.h"

#include "imgui.h"

#include <android/log.h>

#include <atomic>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <mutex>
#include <signal.h>
#include <string>
#include <sys/stat.h>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>
#include <vector>

#define LOG_TAG "AImGui_IME"
#define LOGI(fmt, ...) __android_log_print(ANDROID_LOG_INFO,  LOG_TAG, fmt, ##__VA_ARGS__)
#define LOGW(fmt, ...) __android_log_print(ANDROID_LOG_WARN,  LOG_TAG, fmt, ##__VA_ARGS__)
#define LOGE(fmt, ...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, fmt, ##__VA_ARGS__)

namespace aimgui::ime {

namespace {

pid_t              g_child = -1;
int                g_to_child = -1;
int                g_from_child = -1;
std::thread        g_reader;
std::atomic<bool>  g_running{false};

std::mutex                g_q_mtx;
std::vector<std::string>  g_text_queue;

// Decodes a JSON-style "..." into raw UTF-8.
std::string DecodeJsonString(const std::string& s) {
    std::string out;
    if (s.size() < 2 || s.front() != '"' || s.back() != '"') return out;
    out.reserve(s.size());
    for (size_t i = 1; i + 1 < s.size(); ++i) {
        char c = s[i];
        if (c == '\\' && i + 2 < s.size()) {
            char n = s[i + 1];
            switch (n) {
                case 'n':  out += '\n'; break;
                case 't':  out += '\t'; break;
                case 'r':  out += '\r'; break;
                case '"':  out += '"';  break;
                case '\\': out += '\\'; break;
                case '/':  out += '/';  break;
                default:   out += n;    break;   // skip \uXXXX support
            }
            ++i;
        } else {
            out += c;
        }
    }
    return out;
}

void HandleLine(const std::string& line) {
    if (line.rfind("TEXT ", 0) == 0) {
        std::string text = DecodeJsonString(line.substr(5));
        if (!text.empty()) {
            std::lock_guard<std::mutex> lk(g_q_mtx);
            g_text_queue.push_back(std::move(text));
        }
    } else if (line == "READY" || line == "PONG" ||
               line == "VIEW_OK" || line == "VIEW_OK_FALLBACK") {
        LOGI("helper: %s", line.c_str());
    } else if (line.rfind("ERROR ", 0) == 0) {
        LOGW("helper: %s", line.c_str());
    }
}

void ReaderLoop() {
    char buf[1024];
    std::string line;
    while (g_running.load()) {
        ssize_t n = ::read(g_from_child, buf, sizeof(buf));
        if (n <= 0) {
            if (n == 0)              break;                      // EOF
            if (errno == EINTR)      continue;
            break;
        }
        for (ssize_t i = 0; i < n; ++i) {
            char c = buf[i];
            if (c == '\n') {
                HandleLine(line);
                line.clear();
            } else if (c != '\r') {
                line += c;
            }
        }
    }
    if (!line.empty()) HandleLine(line);
    g_running.store(false);
    LOGI("reader exit");
}

void WriteLine(const char* s) {
    if (g_to_child < 0 || !g_running.load()) return;
    size_t len = std::strlen(s);
    if (::write(g_to_child, s, len) < 0) {
        // Pipe died (helper crashed or was killed). Mark the bridge as
        // dead so subsequent commands no-op immediately.
        g_running.store(false);
        LOGW("write to helper failed: %s", strerror(errno));
        return;
    }
    ::write(g_to_child, "\n", 1);
}

} // namespace

bool Init(const char* dex_path) {
    if (g_running.load()) return true;

    struct stat st;
    if (::stat(dex_path, &st) != 0) {
        LOGW("dex not found: %s", dex_path);
        return false;
    }
    if (::access("/system/bin/app_process", X_OK) != 0) {
        LOGE("/system/bin/app_process not executable");
        return false;
    }

    int in_pipe[2], out_pipe[2];
    if (::pipe(in_pipe)  < 0) return false;
    if (::pipe(out_pipe) < 0) {
        ::close(in_pipe[0]); ::close(in_pipe[1]); return false;
    }

    pid_t pid = ::fork();
    if (pid < 0) {
        ::close(in_pipe[0]);  ::close(in_pipe[1]);
        ::close(out_pipe[0]); ::close(out_pipe[1]);
        return false;
    }

    if (pid == 0) {
        // child: dup pipes onto stdio, exec app_process
        ::dup2(in_pipe[0],  STDIN_FILENO);
        ::dup2(out_pipe[1], STDOUT_FILENO);
        ::dup2(out_pipe[1], STDERR_FILENO);
        ::close(in_pipe[0]);  ::close(in_pipe[1]);
        ::close(out_pipe[0]); ::close(out_pipe[1]);

        std::string dex(dex_path);
        std::string dir = dex;
        auto slash = dir.find_last_of('/');
        if (slash != std::string::npos) dir = dir.substr(0, slash);
        else                            dir = ".";

        std::string cp_arg = "-Djava.class.path=" + dex;

        char* args[] = {
            (char*)"/system/bin/app_process",
            (char*)cp_arg.c_str(),
            (char*)dir.c_str(),
            (char*)"com.aimgui.Helper",
            nullptr,
        };
        ::execv(args[0], args);
        ::_exit(127);
    }

    ::close(in_pipe[0]);
    ::close(out_pipe[1]);
    g_to_child   = in_pipe[1];
    g_from_child = out_pipe[0];
    g_child      = pid;
    g_running.store(true);
    g_reader = std::thread(ReaderLoop);
    LOGI("spawned helper pid=%d dex=%s", (int)pid, dex_path);
    return true;
}

bool IsRunning() { return g_running.load(); }
void Show()      { WriteLine("SHOW"); }
void Hide()      { WriteLine("HIDE"); }

void Flush() {
    std::vector<std::string> drained;
    {
        std::lock_guard<std::mutex> lk(g_q_mtx);
        drained.swap(g_text_queue);
    }
    if (drained.empty()) return;
    ImGuiIO& io = ImGui::GetIO();
    for (const auto& s : drained) io.AddInputCharactersUTF8(s.c_str());
}

void Shutdown() {
    if (!g_running.load() && g_child <= 0) return;
    WriteLine("QUIT");
    g_running.store(false);
    if (g_to_child   >= 0) { ::close(g_to_child);   g_to_child   = -1; }
    if (g_from_child >= 0) { ::close(g_from_child); g_from_child = -1; }
    if (g_reader.joinable()) g_reader.detach();
    if (g_child > 0) {
        ::kill(g_child, SIGTERM);
        ::waitpid(g_child, nullptr, WNOHANG);
        g_child = -1;
    }
}

} // namespace aimgui::ime
