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

// ── 标准库引用（RegisterAppMemory 需要 size_t）─────────
#include <stddef.h>

extern "C" {

/// 注册全局崩溃捕获。重复调用以最后一次为准。
/// Windows: 注册 UnhandledExceptionFilter，崩溃时生成 .dmp 文件
/// Android: 初始化 Google Breakpad ExceptionHandler，崩溃时生成 Minidump
CRASHTRACE_API void StartCrashTrace();

/// 手动触发生成崩溃转储（不触发崩溃）
/// 用于捕获当前进程状态：卡顿检测、内存超标预警、主动调试等场景
/// 返回 true 表示 dump 写入成功
CRASHTRACE_API bool CaptureCrashDump();

/// 注册自定义内存区域到 dump 文件
/// 崩溃时 Breakpad 会将 [ptr, ptr+length) 的内容写入 Minidump
/// 典型用途：保存游戏状态、引擎版本号、用户 ID 等关键上下文
CRASHTRACE_API void RegisterAppMemory(void* ptr, size_t length);

/// 取消注册自定义内存区域
CRASHTRACE_API void UnregisterAppMemory(void* ptr);

} // extern "C"
