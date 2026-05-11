// Windows 平台崩溃捕获实现
#include "../CrashTrace.h"
#include "CrashTrace_windows.h"
#include <iostream>
#include <cstdio>

#pragma comment(lib, "DbgHelp.lib")

// ── 备选过滤器 1：基础堆栈打印 ──────────────────────────
LONG WINAPI TopLevelFilter(_EXCEPTION_POINTERS* excp) {
    // 基础错误信息提取
    DWORD errCode = excp->ExceptionRecord->ExceptionCode;
    PVOID errAddr = excp->ExceptionRecord->ExceptionAddress;

    // 获取完整堆栈（需链接 DbgHelp.lib）
    CONTEXT* ctx = excp->ContextRecord;
    STACKFRAME64 stack = { 0 };
    stack.AddrPC.Offset = ctx->Rip;    // x64 架构
    stack.AddrPC.Mode = AddrModeFlat;

    // 遍历堆栈帧（示例简化为 5 层）
    for (int i = 0; i < 5; i++) {
        if (!StackWalk64(IMAGE_FILE_MACHINE_AMD64,
            GetCurrentProcess(),
            GetCurrentThread(),
            &stack, ctx, nullptr,
            SymFunctionTableAccess64,
            SymGetModuleBase64, nullptr))
            break;
        printf("Stack: %d: 0x%p\n", i, (void*)stack.AddrPC.Offset);
    }

    // 写错误日志
    FILE* f = nullptr;
    fopen_s(&f, "crash.log", "a");
    if (f) {
        fprintf(f, "CRASH [0x%X] at 0x%p\n", errCode, errAddr);
        fclose(f);
    }

    return EXCEPTION_EXECUTE_HANDLER;
}

// ── 备选过滤器 2：安全模式（禁二次异常、禁堆内存）────
LONG WINAPI SafeTopLevelFilter(_EXCEPTION_POINTERS* excp) {
    // 禁用二次异常捕获
    SetUnhandledExceptionFilter(nullptr);

    // 使用静态缓冲区，避免堆分配
    static char buf[1024];
    sprintf_s(buf, "Crash at 0x%p", excp->ExceptionRecord->ExceptionAddress);

    // 直接写磁盘（跳过 C 运行时库）
    HANDLE hFile = CreateFileW(L"crash.dmp", GENERIC_WRITE, 0,
        nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile != INVALID_HANDLE_VALUE) {
        WriteFile(hFile, buf, (DWORD)strlen(buf), nullptr, nullptr);
        CloseHandle(hFile);
    }

    return EXCEPTION_EXECUTE_HANDLER;
}

// ── 备选过滤器 3：仅生成 Minidump ──────────────────────
LONG WINAPI TopLevelFunctionFilter(_EXCEPTION_POINTERS* excp) {
    GenerateMiniDummp(excp, L"crash.dmp");
    return EXCEPTION_EXECUTE_HANDLER;
}

// ── 默认过滤器：时间戳命名 Minidump ────────────────────
LONG WINAPI FuncCrashCallBack(_EXCEPTION_POINTERS* excp) {
    std::time_t t = std::time(0);
    std::wstring filename = L"crash_" + std::to_wstring(t) + L".dmp";
    GenerateMiniDummp(excp, filename.c_str());
    return EXCEPTION_EXECUTE_HANDLER;
}

// ── 堆栈捕获（调试用）─────────────────────────────────
void CaptureStack() {
    HANDLE hProcess = GetCurrentProcess();

    // 初始化符号系统 (需提前调用)
    SymInitialize(hProcess, NULL, TRUE);
    SymSetOptions(SYMOPT_LOAD_LINES | SYMOPT_UNDNAME);

    // 捕获当前线程上下文
    CONTEXT context = {};
    RtlCaptureContext(&context);

    // 初始化堆栈帧结构
    STACKFRAME64 stackFrame = {};
    stackFrame.AddrPC.Offset = context.Rip;
    stackFrame.AddrPC.Mode = AddrModeFlat;
    stackFrame.AddrFrame.Offset = context.Rbp;
    stackFrame.AddrFrame.Mode = AddrModeFlat;

    // 遍历堆栈帧（最多 20 层）
    for (int i = 0; i < 20; i++) {
        if (!StackWalk64(
            IMAGE_FILE_MACHINE_AMD64,
            hProcess, GetCurrentThread(),
            &stackFrame, &context,
            NULL,
            SymFunctionTableAccess64,
            SymGetModuleBase64, NULL)
            )
            break;

        // 获取函数名和行号
        DWORD64 displacement = 0;
        char symbolBuffer[sizeof(IMAGEHLP_SYMBOL64) + 255] = { 0 };
        IMAGEHLP_SYMBOL64* symbol = (IMAGEHLP_SYMBOL64*)symbolBuffer;
        symbol->SizeOfStruct = sizeof(IMAGEHLP_SYMBOL64);
        symbol->MaxNameLength = 255;

        IMAGEHLP_LINE64 line = { sizeof(IMAGEHLP_LINE64) };

        if (SymGetSymFromAddr64(hProcess, stackFrame.AddrPC.Offset, &displacement, symbol)) {
            std::cout << symbol->Name << "+0x" << std::hex << displacement;
            if (SymGetLineFromAddr64(hProcess, stackFrame.AddrPC.Offset, (PDWORD)&displacement, &line)) {
                std::cout << "  " << line.FileName << ":" << line.LineNumber << '\n';
            }
        }
    }
    SymCleanup(hProcess);
}

// ── Minidump 生成 ─────────────────────────────────────
void GenerateMiniDummp(_EXCEPTION_POINTERS* excp, LPCWSTR path) {
    HANDLE hFile = CreateFileW(
        path,
        GENERIC_WRITE,
        0,
        nullptr,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        nullptr
    );

    if (hFile == INVALID_HANDLE_VALUE) {
        return;
    }

    MINIDUMP_EXCEPTION_INFORMATION dumpInfo;
    dumpInfo.ThreadId = GetCurrentThreadId();
    dumpInfo.ExceptionPointers = excp;
    dumpInfo.ClientPointers = FALSE;

    MiniDumpWriteDump(
        GetCurrentProcess(),
        GetCurrentProcessId(),
        hFile,
        static_cast<MINIDUMP_TYPE>(
            MiniDumpWithFullMemory |
            MiniDumpWithHandleData |
            MiniDumpWithProcessThreadData),
        &dumpInfo,
        nullptr,
        nullptr
    );
    CloseHandle(hFile);
}

// ── 公共 API 实现 ─────────────────────────────────────
void StartCrashTrace() {
    SetUnhandledExceptionFilter(FuncCrashCallBack);
}

// ── 主动写 dump（非崩溃场景）─────────────────────────
// 模拟 EXCEPTION_POINTERS 以复用 Minidump 生成逻辑
bool CaptureCrashDumpWindows() {
    // 在当前线程触发一个虚假的断点异常以捕获上下文
    __try {
        DebugBreak();
    } __except (GenerateMiniDummp(
        GetExceptionInformation(),
        (std::wstring(L"capture_") + std::to_wstring(std::time(0)) + L".dmp").c_str()
    ), EXCEPTION_EXECUTE_HANDLER) {
    }
    return true;
}

// ── Windows 端暂不持久化 AppMemory ──────────────────────
// Minidump 本身已包含完整进程内存，游戏状态可通过
// MiniDumpWriteDump 的 UserStreamParam 参数注入
void RegisterAppMemoryWindows(void*, size_t) {}
void UnregisterAppMemoryWindows(void*) {}

// ── 公共 API 调度（见 CrashTrace.h）─────────────────────
bool CaptureCrashDump() {
    return CaptureCrashDumpWindows();
}

void RegisterAppMemory(void* ptr, size_t length) {
    RegisterAppMemoryWindows(ptr, length);
}

void UnregisterAppMemory(void* ptr) {
    UnregisterAppMemoryWindows(ptr);
}