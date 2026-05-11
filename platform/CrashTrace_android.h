// Android 平台 — Google Breakpad 集成
#pragma once

#include <android/log.h>
#include <signal.h>

#include "client/linux/handler/exception_handler.h"
#include "client/linux/handler/minidump_descriptor.h"

// 全局异常处理器（进程存活期内保持有效）
extern google_breakpad::ExceptionHandler* g_breakpad_handler;

// Minidump 写入完成回调
bool DumpCallback(const google_breakpad::MinidumpDescriptor& descriptor,
                  void* context, bool succeeded);

// 崩溃预处理过滤器（返回 true 才写 dump）
bool FilterCallback(void* context);

// 捕获当前线程调用栈（非崩溃调试用，输出到 logcat）
void CaptureBacktrace();
