// CrashTrace 平台调度
// 按编译目标平台选择对应实现
#include "CrashTrace.h"

#if defined(CRASHTRACE_PLATFORM_WINDOWS)
  #include "platform/CrashTrace_windows.h"
#elif defined(CRASHTRACE_PLATFORM_ANDROID)
  #include "platform/CrashTrace_android.h"
#endif

// ── 平台入口点 ─────────────────────────────────────────

#if defined(CRASHTRACE_PLATFORM_WINDOWS)

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    switch (ul_reason_for_call) {
    case DLL_PROCESS_ATTACH:
        DisableThreadLibraryCalls(hModule);
        break;
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
    default:
        break;
    }
    return TRUE;
}

// Windows 端 stub — 主动 dump 能力已在 CrashTrace_windows.cpp 中实现
// CaptureCrashDump / RegisterAppMemory / UnregisterAppMemory 见平台实现

#elif defined(CRASHTRACE_PLATFORM_ANDROID)

#include <jni.h>

JNIEXPORT jint JNI_OnLoad(JavaVM* vm, void* /*reserved*/) {
    // 库加载时自动注册 Breakpad 信号处理器
    // 注意：如需传入 JavaVM 指针到回调中，可在此处保存为全局变量
    StartCrashTrace();
    return JNI_VERSION_1_6;
}

#endif
