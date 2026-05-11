// Android 平台内部声明（预埋）
#pragma once

#include <signal.h>
#include <unwind.h>
#include <dlfcn.h>

// Android 崩溃信号回调
void AndroidCrashHandler(int sig);

// 捕获并打印 backtrace
void CaptureBacktrace();