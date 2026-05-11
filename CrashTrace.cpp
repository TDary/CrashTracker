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

#elif defined(CRASHTRACE_PLATFORM_ANDROID)

#include <jni.h>

JNIEXPORT jint JNI_OnLoad(JavaVM* vm, void* reserved) {
    // 库加载时自动注册信号处理器
    StartCrashTrace();
    return JNI_VERSION_1_6;
}

#endif