// Android 平台 — Google Breakpad 崩溃捕获实现
//
// 核心流程：
//   sigaction(SA_SIGINFO) → SignalHandler → HandleSignal
//     → clone() 子进程 → ptrace 父进程
//       → WriteMinidump(.dmp) → DumpCallback
//
// 参考：Google Breakpad 官方文档及《安卓游戏宕机捕获与解析全链路实战教程》
#include "CrashTrace_android.h"
#include "../CrashTrace.h"

#include <unistd.h>

// ── 配置常量 ───────────────────────────────────────────
// Minidump 输出目录（游戏上线建议改为 /data/data/<pkg>/files/minidumps）
static const char* kDumpDirectory = "/data/local/tmp/crash_dumps";

// 单个 dump 文件大小上限（游戏场景 2MB 足够，避免 OOM）
static const off_t kDumpSizeLimit = 2 * 1024 * 1024;

// ── 全局处理器实例 ────────────────────────────────────
// 必须保持在进程存活期内有效；析构时会自动恢复原始信号处理器
google_breakpad::ExceptionHandler* g_breakpad_handler = nullptr;

// ── 信号名映射（logcat 输出用）───────────────────────
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

// ── Minidump 写入完成回调 ──────────────────────────────
// 运行在 clone() 子进程中，堆/栈安全
// 返回 true 表示异常已处理，进程将终止；false 则继续传递信号
bool DumpCallback(const google_breakpad::MinidumpDescriptor& descriptor,
                  void* /*context*/, bool succeeded) {
    __android_log_print(ANDROID_LOG_ERROR, "CrashTrace",
        "========================================");
    __android_log_print(ANDROID_LOG_ERROR, "CrashTrace",
        "CRASH DUMP: %s", succeeded ? "SUCCESS" : "FAILED");
    __android_log_print(ANDROID_LOG_ERROR, "CrashTrace",
        "Path: %s", descriptor.path());
    __android_log_print(ANDROID_LOG_ERROR, "CrashTrace",
        "Size limit: %lld bytes", (long long)descriptor.size_limit());
    __android_log_print(ANDROID_LOG_ERROR, "CrashTrace",
        "========================================");

    // TODO: 在此处接入上传逻辑（参考 PDF 教程第 4 章 — HTTP 上传到符号服务器）
    // google_breakpad::HTTPUpload::SendRequest(...)

    return succeeded;
}

// ── 崩溃预处理过滤器 ──────────────────────────────────
// 运行在崩溃线程的信号处理器上下文中（compromised context）
// 返回 true = 继续生成 minidump；false = 跳过
bool FilterCallback(void* /*context*/) {
    return true;
}

// ── 捕获当前线程调用栈（调试用）───────────────────────
// 需要 libunwind 支持，非崩溃场景可安全使用 malloc
void CaptureBacktrace() {
    if (!g_breakpad_handler) {
        __android_log_print(ANDROID_LOG_WARN, "CrashTrace",
            "CaptureBacktrace: handler not initialized, call StartCrashTrace() first");
        return;
    }

    // Breakpad 的 WriteMinidump 会在非崩溃场景下生成完整 dump
    // 如需轻量级 backtrace，可继续使用 _Unwind_Backtrace
    __android_log_print(ANDROID_LOG_DEBUG, "CrashTrace",
        "Writing manual minidump...");
    g_breakpad_handler->WriteMinidump();
}

// ── 公共 API ──────────────────────────────────────────

void StartCrashTrace() {
    // 防止重复初始化
    if (g_breakpad_handler) {
        __android_log_print(ANDROID_LOG_WARN, "CrashTrace",
            "StartCrashTrace: already initialized");
        return;
    }

    __android_log_print(ANDROID_LOG_INFO, "CrashTrace",
        "Initializing Google Breakpad crash handler...");
    __android_log_print(ANDROID_LOG_INFO, "CrashTrace",
        "Dump directory: %s", kDumpDirectory);

    google_breakpad::MinidumpDescriptor descriptor(kDumpDirectory);
    descriptor.set_size_limit(kDumpSizeLimit);

    g_breakpad_handler = new google_breakpad::ExceptionHandler(
        descriptor,
        FilterCallback,   // 可替换为 nullptr 跳过过滤
        DumpCallback,
        nullptr,          // callback_context
        true,             // install_handler — 自动注册 sigaction
        -1                // server_fd = -1 — 进程内 dump 生成
    );

    __android_log_print(ANDROID_LOG_INFO, "CrashTrace",
        "Breakpad handler installed — monitoring SIGSEGV/SIGABRT/SIGBUS/"
        "SIGFPE/SIGILL/SIGTRAP");
}

bool CaptureCrashDump() {
    if (!g_breakpad_handler) {
        __android_log_print(ANDROID_LOG_WARN, "CrashTrace",
            "CaptureCrashDump: handler not initialized");
        return false;
    }
    return g_breakpad_handler->WriteMinidump();
}

void RegisterAppMemory(void* ptr, size_t length) {
    if (!g_breakpad_handler) {
        __android_log_print(ANDROID_LOG_WARN, "CrashTrace",
            "RegisterAppMemory: handler not initialized");
        return;
    }
    g_breakpad_handler->RegisterAppMemory(ptr, length);
}

void UnregisterAppMemory(void* ptr) {
    if (!g_breakpad_handler) {
        __android_log_print(ANDROID_LOG_WARN, "CrashTrace",
            "UnregisterAppMemory: handler not initialized");
        return;
    }
    g_breakpad_handler->UnregisterAppMemory(ptr);
}
