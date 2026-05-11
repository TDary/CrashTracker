// CrashTrace 公共接口
// 跨平台崩溃追踪 SDK — Windows / Android
#pragma once

// ── 平台检测 ────────────────────────────────────────────
#if defined(_WIN32) || defined(_WIN64)
  #define CRASHTRACE_PLATFORM_WINDOWS 1
#elif defined(__ANDROID__)
  #define CRASHTRACE_PLATFORM_ANDROID 1
#else
  #error "CrashTrace: unsupported platform"
#endif

// ── DLL / SO 导出控制 ───────────────────────────────────
#if defined(CRASHTRACE_PLATFORM_WINDOWS)
  #ifdef CrashTrace_EXPORTS
    #define CRASHTRACE_API __declspec(dllexport)
  #else
    #define CRASHTRACE_API __declspec(dllimport)
  #endif
#elif defined(CRASHTRACE_PLATFORM_ANDROID)
  #define CRASHTRACE_API __attribute__((visibility("default")))
#endif

extern "C" {

/// 注册全局崩溃捕获。重复调用以最后一次为准。
/// Windows: 注册 UnhandledExceptionFilter，崩溃时生成 .dmp 文件
/// Android: 注册 sigaction 信号处理器，崩溃时输出 backtrace（待实现）
CRASHTRACE_API void StartCrashTrace();

} // extern "C"