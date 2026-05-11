// Android 平台崩溃捕获实现（预埋）
#include "CrashTrace_android.h"
#include "../CrashTrace.h"

// ── 信号 → 名称映射 ──────────────────────────────────
static const char* SignalName(int sig) {
    switch (sig) {
    case SIGABRT: return "SIGABRT";
    case SIGSEGV: return "SIGSEGV";
    case SIGBUS:  return "SIGBUS";
    case SIGFPE:  return "SIGFPE";
    case SIGILL:  return "SIGILL";
    case SIGTRAP: return "SIGTRAP";
    default:      return "UNKNOWN";
    }
}

// ── backtrace 帧回调 ─────────────────────────────────
static _Unwind_Reason_Code BacktraceCallback(_Unwind_Context* ctx, void* arg) {
    int* count = static_cast<int*>(arg);
    void* ip = reinterpret_cast<void*>(_Unwind_GetIP(ctx));
    if (ip && *count < 64) {
        Dl_info info;
        if (dladdr(ip, &info) && info.dli_sname) {
            __android_log_print(ANDROID_LOG_ERROR, "CrashTrace",
                "  #%02d  %s (%s+0x%lx)",
                *count,
                info.dli_fname ? info.dli_fname : "?",
                info.dli_sname,
                reinterpret_cast<uintptr_t>(ip) - reinterpret_cast<uintptr_t>(info.dli_saddr));
        } else {
            __android_log_print(ANDROID_LOG_ERROR, "CrashTrace",
                "  #%02d  %p", *count, ip);
        }
        (*count)++;
    }
    return _URC_NO_REASON;
}

// ── 崩溃信号处理 ─────────────────────────────────────
void AndroidCrashHandler(int sig) {
    // 避免递归崩溃
    static volatile sig_atomic_t in_handler = 0;
    if (in_handler) _exit(1);
    in_handler = 1;

    __android_log_print(ANDROID_LOG_ERROR, "CrashTrace",
        "========================================");
    __android_log_print(ANDROID_LOG_ERROR, "CrashTrace",
        "CRASH: signal %d (%s)", sig, SignalName(sig));
    __android_log_print(ANDROID_LOG_ERROR, "CrashTrace",
        "========================================");

    int count = 0;
    _Unwind_Backtrace(BacktraceCallback, &count);

    // 恢复默认处理，重新触发信号以生成 tombstone
    signal(sig, SIG_DFL);
    raise(sig);
}

// ── 捕获当前线程 backtrace（调试用）─────────────────
void CaptureBacktrace() {
    __android_log_print(ANDROID_LOG_DEBUG, "CrashTrace", "=== Backtrace ===");
    int count = 0;
    _Unwind_Backtrace(BacktraceCallback, &count);
}

// ── 公共 API 实现 ─────────────────────────────────────
void StartCrashTrace() {
    struct sigaction sa;
    sa.sa_handler = AndroidCrashHandler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;

    sigaction(SIGABRT, &sa, nullptr);
    sigaction(SIGSEGV, &sa, nullptr);
    sigaction(SIGBUS,  &sa, nullptr);
    sigaction(SIGFPE,  &sa, nullptr);
    sigaction(SIGILL,  &sa, nullptr);
    sigaction(SIGTRAP, &sa, nullptr);
}