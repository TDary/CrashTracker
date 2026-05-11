# CrashTrace

跨平台崩溃追踪 SDK — 支持 Windows (DLL) / Android (SO)。

## 架构

```
CrashTrace/
├── CrashTrace.h                  # 公共接口（StartCrashTrace）
├── CrashTrace.cpp                # 平台调度 + 入口点（DllMain / JNI_OnLoad）
├── framework.h / .cpp            # 预编译头适配
├── CMakeLists.txt                # 构建系统（自动按平台选择源文件）
├── platform/
│   ├── CrashTrace_windows.h      # Windows 内部声明（异常过滤器 / Minidump / 堆栈）
│   ├── CrashTrace_windows.cpp    # Windows 完整实现
│   ├── CrashTrace_android.h      # Android 内部声明（信号处理 / backtrace）
│   └── CrashTrace_android.cpp    # Android 实现（预埋，功能可用）
│── backward-cpp-1.6/             # 第三方：backward-cpp 堆栈解析库（备用）
└── out/
```

**平台分发逻辑**：`CrashTrace.h` 通过 `#if defined(_WIN32) ... #elif defined(__ANDROID__)` 自动选择平台头文件；CMake 通过 `if(WIN32) ... elseif(ANDROID)` 条件编译对应源文件。新增平台只需添加 `platform/CrashTrace_xxx.h/.cpp` 并在 CMake 增加一个分支。

## 功能

| 平台 | 崩溃捕获 | 输出 | 状态 |
|------|---------|------|------|
| Windows | `SetUnhandledExceptionFilter` + `MiniDumpWriteDump` | `.dmp` 文件（带时间戳） | 完成 |
| Android | `sigaction` + `_Unwind_Backtrace` + `dladdr` | logcat + tombstone | 预埋 |

### Windows 详细

- 默认过滤器 `FuncCrashCallBack`：崩溃时生成 `crash_<timestamp>.dmp`，可用 WinDbg / VS 打开
- 备选过滤器可切换：`TopLevelFilter`（日志式）、`SafeTopLevelFilter`（安全模式）、`TopLevelFunctionFilter`（纯 dmp）
- `CaptureStack()` 可在不崩溃时主动打印当前调用栈（调试用）

### Android 详细（预埋）

- 注册 `SIGABRT / SIGSEGV / SIGBUS / SIGFPE / SIGILL / SIGTRAP`
- 崩溃时通过 `_Unwind_Backtrace` 解析调用栈并输出到 logcat
- 随后恢复默认处理器并 re-raise 信号，让系统生成 tombstone
- `JNI_OnLoad` 自动注册，Java 层 `System.loadLibrary("CrashTrace")` 即启用

## 构建

### Windows（x64 Debug）

```bash
cmake -B out/build/x64-debug -G Ninja \
  -DCMAKE_C_COMPILER=cl.exe \
  -DCMAKE_CXX_COMPILER=cl.exe \
  -DCMAKE_BUILD_TYPE=Debug
cmake --build out/build/x64-debug
```

产物：`out/build/x64-debug/CrashTrace.dll`

### Android（交叉编译，待 NDK 接入）

```bash
cmake -B out/build/android-arm64 -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE=$ANDROID_NDK/build/cmake/android.toolchain.cmake \
  -DANDROID_ABI=arm64-v8a \
  -DANDROID_PLATFORM=android-21 \
  -DCMAKE_BUILD_TYPE=Release
cmake --build out/build/android-arm64
```

产物：`out/build/android-arm64/libCrashTrace.so`

## 使用

```cpp
#include "CrashTrace.h"

int main() {
    StartCrashTrace();  // 全局注册崩溃捕获，仅需调用一次

    // ... 业务代码 ...
    int* p = nullptr;
    *p = 42;  // 触发崩溃 → crash_<timestamp>.dmp
    return 0;
}
```

> 注意：`StartCrashTrace()` 内部仅注册异常处理器，不分配堆内存，可在 `main()` 第一行安全调用。

## 添加新平台

1. 创建 `platform/CrashTrace_<platform>.h` — 声明平台内部函数
2. 创建 `platform/CrashTrace_<platform>.cpp` — 实现 `StartCrashTrace()` + 崩溃捕获逻辑
3. `CrashTrace.h` 中增加 `#elif` 分支
4. `CMakeLists.txt` 中增加 `elseif(PLATFORM)` 分支，添加源文件和链接库
5. `CrashTrace.cpp` 中增加平台入口点（如 `DllMain` / `JNI_OnLoad` / `__attribute__((constructor))`）