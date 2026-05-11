# CrashTrace — 跨平台崩溃追踪 SDK
# Windows (DLL) / Android (SO)
cmake_minimum_required(VERSION 3.8)

# 如果支持，请为 MSVC 编译器启用热重载。
if(POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

project("CrashTrace")

# ── 平台检测 ──────────────────────────────────────────
if(WIN32)
  set(PLATFORM_SOURCES
    "platform/CrashTrace_windows.h"
    "platform/CrashTrace_windows.cpp"
  )
  set(PLATFORM_LIBS DbgHelp)
elseif(ANDROID)
  set(PLATFORM_SOURCES
    "platform/CrashTrace_android.h"
    "platform/CrashTrace_android.cpp"
  )
  set(PLATFORM_LIBS log)
else()
  message(FATAL_ERROR "CrashTrace: unsupported platform (only Windows / Android)")
endif()

# ── 公共源文件 ────────────────────────────────────────
set(COMMON_SOURCES
  "CrashTrace.h"
  "CrashTrace.cpp"
  "framework.h"
  "framework.cpp"
)

# ── 库目标 ────────────────────────────────────────────
add_library(CrashTrace SHARED ${COMMON_SOURCES} ${PLATFORM_SOURCES})

target_include_directories(CrashTrace PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(CrashTrace PRIVATE CrashTrace_EXPORTS)

if(MSVC)
  target_compile_options(CrashTrace PRIVATE /utf-8)
endif()

# ── 链接平台库 ────────────────────────────────────────
if(PLATFORM_LIBS)
  target_link_libraries(CrashTrace PRIVATE ${PLATFORM_LIBS})
endif()

# ── 标准 ─────────────────────────────────────────────
if(CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET CrashTrace PROPERTY CXX_STANDARD 20)
endif()