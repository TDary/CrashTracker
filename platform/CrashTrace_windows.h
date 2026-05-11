// Windows 平台内部声明
#pragma once

#include <windows.h>
#include <DbgHelp.h>
#include <ctime>
#include <string>

// 异常回调（注册给 SetUnhandledExceptionFilter）
LONG WINAPI FuncCrashCallBack(_EXCEPTION_POINTERS* excp);

// 生成 Minidump
void GenerateMiniDummp(_EXCEPTION_POINTERS* excp, LPCWSTR path);

// 捕获并打印堆栈（调试用）
void CaptureStack();

// 主动写 dump（不崩溃场景）
bool CaptureCrashDumpWindows();

// 注册/取消注册自定义内存区域（Windows 端暂存到全局列表）
void RegisterAppMemoryWindows(void* ptr, size_t length);
void UnregisterAppMemoryWindows(void* ptr);

// 备选异常过滤器（可按需切换）
LONG WINAPI SafeTopLevelFilter(_EXCEPTION_POINTERS* excp);
LONG WINAPI TopLevelFilter(_EXCEPTION_POINTERS* excp);
LONG WINAPI TopLevelFunctionFilter(_EXCEPTION_POINTERS* excp);
