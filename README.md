# CrashTrace

跨平台崩溃追踪 SDK — 支持 Windows (DLL) / Android (SO)。
Android 端基于 Google Breakpad 实现完整的 Minidump 生成与信号安全捕获。

## 架构

```
CrashTrace/
├── CrashTrace.h                  # 公共接口（StartCrashTrace / CaptureCrashDump / RegisterAppMemory）
├── CrashTrace.cpp                # 平台调度 + 入口点（DllMain / JNI_OnLoad）
├── framework.h / .cpp            # Windows 预编译头适配（仅 Windows 构建包含）
├── CMakeLists.txt                # 构建系统（Windows DLL / Android SO + breakpad_client 静态库）
├── platform/
│   ├── CrashTrace_windows.h      # Windows 内部声明（异常过滤器 / Minidump / 堆栈）
│   ├── CrashTrace_windows.cpp    # Windows 完整实现（SetUnhandledExceptionFilter + MiniDumpWriteDump）
│   ├── CrashTrace_android.h      # Android 内部声明（Breakpad ExceptionHandler 集成）
│   └── CrashTrace_android.cpp    # Android Breakpad 实现（信号安全 Minidump 生成）
├── breakpad/                     # Google Breakpad 源码（LGPL-style）
│   └── src/
│       ├── client/linux/         # Linux/Android 客户端（ExceptionHandler / Minidump Writer）
│       ├── client/minidump_file_writer.cc  # Minidump 文件格式写入器
│       ├── common/               # 跨平台基础（MD5 / UTF / Linux 工具）
│       └── third_party/lss/      # Linux Syscall Support（单头文件）
├── backward-cpp-1.6/             # 第三方：backward-cpp 堆栈解析库（备用）
└── out/
```

**平台分发逻辑**：`CrashTrace.h` 通过 `#if defined(_WIN32) ... #elif defined(__ANDROID__)` 自动选择平台头文件；CMake 通过 `if(WIN32) ... elseif(ANDROID)` 条件编译对应源文件。

## 功能

| 平台 | 崩溃捕获 | 输出 | 状态 |
|------|---------|------|------|
| Windows | `SetUnhandledExceptionFilter` + `MiniDumpWriteDump` | `.dmp` 文件（带时间戳） | 完成 |
| Android | Breakpad `ExceptionHandler` + `sigaction(SA_SIGINFO)` → `clone()` + `ptrace` | Minidump（`.dmp`）+ logcat | 完成 |

### Android Breakpad 详细

基于 Google Breakpad 客户端库实现，关键特性：

- **信号安全**：信号处理器运行在替代栈上，通过 `clone()` 创建子进程，子进程用 `ptrace` 读取父进程内存写 dump，避免污染崩溃线程
- **绕过 Android bug**：直接使用 `sys_rt_sigaction` 绕过 Android L+ 的 `sigaction(SIG_DFL)` 失效问题
- **dump 大小限制**：默认 2MB（游戏场景避免 OOM），可通过 `MinidumpDescriptor::set_size_limit()` 调整
- **回调链**：
  - `FilterCallback` — 崩溃后写 dump 前的预处理，返回 false 可跳过 dump
  - `DumpCallback` — dump 写完后触发，可用于上传服务器（参考 PDF 教程第 4 章）
- **主动 dump**：`CaptureCrashDump()` 在不崩溃时生成当前进程快照（卡顿检测、调试用）
- **自定义内存区域**：`RegisterAppMemory()` 将游戏状态数据注入 dump 文件
- `JNI_OnLoad` 自动注册，Java 层 `System.loadLibrary("CrashTrace")` 即启用

### Windows 详细

- 默认过滤器 `FuncCrashCallBack`：崩溃时生成 `crash_<timestamp>.dmp`，可用 WinDbg / VS 打开
- 备选过滤器可切换：`TopLevelFilter`（日志式）、`SafeTopLevelFilter`（安全模式）、`TopLevelFunctionFilter`（纯 dmp）
- `CaptureStack()` 可在不崩溃时主动打印当前调用栈（调试用）
- `CaptureCrashDumpWindows()` 主动写 dump（`CaptureCrashDump()` 调度）

## 构建

### Windows

#### 方式一：Visual Studio IDE（推荐）

1. 用 VS 2022 打开 `F:\CrashTracker` 文件夹（File → Open → Folder...）
2. VS 会自动检测 `CMakeLists.txt` 并生成 CMake 缓存
3. 如果修改了 `CMakeLists.txt`，VS 会提示重新生成，或手动：**项目 → 配置缓存**
4. **生成 → 全部生成**（Ctrl+Shift+B）
5. 产物：`out/build/x64-debug/CrashTrace.dll`

> 需要在 VS Installer 中安装"用于 Windows 的 C++ CMake 工具"组件。

#### 方式二：命令行 CMake Presets

打开 **Developer Command Prompt for VS 2022**（开始菜单搜索）：

```cmd
REM 配置 + 构建 x64 Debug
cmake --preset x64-debug -S F:\CrashTracker
cmake --build out/build/x64-debug

REM 配置 + 构建 x64 Release
cmake --preset x64-release -S F:\CrashTracker
cmake --build out/build/x64-release
```

产物：`out/build/x64-debug/CrashTrace.dll`

### Android（交叉编译，需 NDK）

#### 前置条件

| 组件 | 版本 | 说明 |
|------|------|------|
| Android NDK | **r27d** | `F:/android-ndk-r27d-windows/android-ndk-r27d` |
| Toolchain | LLVM Clang 18.0.4 | `toolchains/llvm/prebuilt/windows-x86_64` |
| CMake | 3.8+ | 交叉编译使用 NDK 自带的 `android.toolchain.cmake` |

> 如 NDK 路径不同，请修改 `CMakePresets.json` 中 `android-base` preset 的 `CMAKE_TOOLCHAIN_FILE` 变量。
> 验证 NDK 版本：`file out/build/android-arm64-release/libCrashTrace.so` 输出中可见 `built by NDK r27d`。

#### 构建（使用 CMake Presets）

```bash
# arm64-v8a（64 位，主流设备）
cmake --preset android-arm64-release -S F:\CrashTracker
cmake --build out/build/android-arm64-release

# armeabi-v7a（32 位兼容）
cmake --preset android-arm32-release -S F:\CrashTracker
cmake --build out/build/android-arm32-release
```

产物：
- `out/build/android-arm64-release/libCrashTrace.so`
- `out/build/android-arm32-release/libCrashTrace.so`

支持的 ABI：通过修改 preset 中 `ANDROID_ABI` 可构建 `armeabi-v7a` / `arm64-v8a` / `x86` / `x86_64`

## 使用

```cpp
#include "CrashTrace.h"

int main() {
    StartCrashTrace();  // 全局注册崩溃捕获，仅需调用一次

    // 可选：注册游戏关键状态到 dump
    int gameState = 42;
    RegisterAppMemory(&gameState, sizeof(gameState));

    // ... 业务代码 ...
    int* p = nullptr;
    *p = 42;  // 触发崩溃 → Android: /data/local/tmp/crash_dumps/<guid>.dmp
              //             Windows: crash_<timestamp>.dmp
    return 0;
}
```

### Android 接入（Unity 游戏）

```java
// MainActivity.java
public class MainActivity extends Activity {
    static {
        System.loadLibrary("CrashTrace");  // 加载即自动 StartCrashTrace()
    }
}
```

Unity 中放置 .so 的位置：
```
Assets/Plugins/Android/
├── arm64-v8a/
│   └── libCrashTrace.so
└── armeabi-v7a/
    └── libCrashTrace.so
```

> `JNI_OnLoad` 自动调用 `StartCrashTrace()`，无需手动初始化。

## 添加新平台

1. 创建 `platform/CrashTrace_<platform>.h` — 声明平台内部函数
2. 创建 `platform/CrashTrace_<platform>.cpp` — 实现 `StartCrashTrace()` + 崩溃捕获逻辑
3. `CrashTrace.h` 中增加 `#elif` 分支
4. `CMakeLists.txt` 中增加 `elseif(PLATFORM)` 分支，添加源文件和链接库
5. `CrashTrace.cpp` 中增加平台入口点（如 `DllMain` / `JNI_OnLoad` / `__attribute__((constructor))`）

## 符号解析与 Minidump 分析

Android 端生成的 `.dmp` 文件可通过 Breakpad 工具链分析：

```bash
# 1. 从带符号的 .so 导出符号文件
dump_syms libCrashTrace.so > CrashTrace.sym

# 2. 用 minidump_stackwalk 解析崩溃堆栈
minidump_stackwalk <dump文件>.dmp ./symbols/ > crash_report.txt
```

详见 Breakpad 官方文档 `breakpad/docs/`。
