# CrashTrace — 跨平台崩溃追踪 SDK
# Windows (DLL) / Android (SO)
# 集成 Google Breakpad 客户端库（仅 Android）
cmake_minimum_required(VERSION 3.8)

# 如果支持，请为 MSVC 编译器启用热重载。
if(POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

project("CrashTrace")

# ── 公共源文件 ────────────────────────────────────────
set(COMMON_SOURCES
  "CrashTrace.h"
  "CrashTrace.cpp"
  "framework.h"
  "framework.cpp"
)

# ── 平台检测 ──────────────────────────────────────────
if(WIN32)
  set(PLATFORM_SOURCES
    "platform/CrashTrace_windows.h"
    "platform/CrashTrace_windows.cpp"
  )
  set(PLATFORM_LIBS DbgHelp)

  # Windows Dll 目标
  add_library(CrashTrace SHARED ${COMMON_SOURCES} ${PLATFORM_SOURCES})
  # 将 Android 源文件也纳入 target（仅头文件用于 IDE 索引，.cpp 不编译）
  target_sources(CrashTrace PRIVATE
    "platform/CrashTrace_android.h"
    "platform/CrashTrace_android.cpp"
  )
  set_source_files_properties(
    "platform/CrashTrace_android.cpp"
    PROPERTIES HEADER_FILE_ONLY TRUE
  )
  target_include_directories(CrashTrace PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
  target_compile_definitions(CrashTrace PRIVATE CrashTrace_EXPORTS)
  if(MSVC)
    target_compile_options(CrashTrace PRIVATE /utf-8)
  endif()
  target_link_libraries(CrashTrace PRIVATE ${PLATFORM_LIBS})

elseif(ANDROID)
  # ── Breakpad 静态库 ─────────────────────────────────
  set(BREAKPAD_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/breakpad")

  # Breakpad client 源文件（与官方 android/google_breakpad/Android.mk 对齐）
  set(BREAKPAD_CLIENT_SOURCES
    # Linux handler
    "${BREAKPAD_ROOT}/src/client/linux/handler/exception_handler.cc"
    "${BREAKPAD_ROOT}/src/client/linux/handler/minidump_descriptor.cc"
    # Linux minidump writer
    "${BREAKPAD_ROOT}/src/client/linux/minidump_writer/linux_dumper.cc"
    "${BREAKPAD_ROOT}/src/client/linux/minidump_writer/linux_ptrace_dumper.cc"
    "${BREAKPAD_ROOT}/src/client/linux/minidump_writer/minidump_writer.cc"
    "${BREAKPAD_ROOT}/src/client/linux/minidump_writer/pe_file.cc"
    # Linux dump writer common
    "${BREAKPAD_ROOT}/src/client/linux/dump_writer_common/thread_info.cc"
    "${BREAKPAD_ROOT}/src/client/linux/dump_writer_common/ucontext_reader.cc"
    # Linux microdump writer
    "${BREAKPAD_ROOT}/src/client/linux/microdump_writer/microdump_writer.cc"
    # Linux crash generation (out-of-process dump)
    "${BREAKPAD_ROOT}/src/client/linux/crash_generation/crash_generation_client.cc"
    # Linux log
    "${BREAKPAD_ROOT}/src/client/linux/log/log.cc"
    # Common — minidump file writer
    "${BREAKPAD_ROOT}/src/client/minidump_file_writer.cc"
    # Common — string / encoding
    "${BREAKPAD_ROOT}/src/common/convert_UTF.cc"
    "${BREAKPAD_ROOT}/src/common/md5.cc"
    "${BREAKPAD_ROOT}/src/common/string_conversion.cc"
    # Common — Linux specific
    "${BREAKPAD_ROOT}/src/common/linux/elfutils.cc"
    "${BREAKPAD_ROOT}/src/common/linux/file_id.cc"
    "${BREAKPAD_ROOT}/src/common/linux/guid_creator.cc"
    "${BREAKPAD_ROOT}/src/common/linux/linux_libc_support.cc"
    "${BREAKPAD_ROOT}/src/common/linux/memory_mapped_file.cc"
    "${BREAKPAD_ROOT}/src/common/linux/safe_readlink.cc"
  )

  # Breakpad ARM/ARM64 getcontext 汇编
  if(CMAKE_ANDROID_ARCH_ABI STREQUAL "armeabi-v7a")
    set(BREAKPAD_GETCONTEXT_ASM
      "${BREAKPAD_ROOT}/src/common/linux/breakpad_getcontext.S")
    set_source_files_properties(${BREAKPAD_GETCONTEXT_ASM}
      PROPERTIES COMPILE_FLAGS "-x assembler-with-cpp")
  elseif(CMAKE_ANDROID_ARCH_ABI STREQUAL "arm64-v8a")
    set(BREAKPAD_GETCONTEXT_ASM
      "${BREAKPAD_ROOT}/src/common/linux/breakpad_getcontext.S")
    set_source_files_properties(${BREAKPAD_GETCONTEXT_ASM}
      PROPERTIES COMPILE_FLAGS "-x assembler-with-cpp")
  elseif(CMAKE_ANDROID_ARCH_ABI STREQUAL "x86")
    set(BREAKPAD_GETCONTEXT_ASM
      "${BREAKPAD_ROOT}/src/common/linux/breakpad_getcontext.S")
    set_source_files_properties(${BREAKPAD_GETCONTEXT_ASM}
      PROPERTIES COMPILE_FLAGS "-x assembler-with-cpp")
  elseif(CMAKE_ANDROID_ARCH_ABI STREQUAL "x86_64")
    set(BREAKPAD_GETCONTEXT_ASM
      "${BREAKPAD_ROOT}/src/common/linux/breakpad_getcontext.S")
    set_source_files_properties(${BREAKPAD_GETCONTEXT_ASM}
      PROPERTIES COMPILE_FLAGS "-x assembler-with-cpp")
  endif()

  # breakpad_client 静态库
  add_library(breakpad_client STATIC
    ${BREAKPAD_CLIENT_SOURCES}
    ${BREAKPAD_GETCONTEXT_ASM}
  )

  target_include_directories(breakpad_client PUBLIC
    "${BREAKPAD_ROOT}/src/common/android/include"   # Android STL 兼容（elf.h, link.h...）
    "${BREAKPAD_ROOT}/src"                           # google_breakpad 公共头
    "${BREAKPAD_ROOT}/src/third_party/lss"           # LSS syscall 封装
  )

  target_compile_definitions(breakpad_client PRIVATE
    __STDC_FORMAT_MACROS
    __STDC_LIMIT_MACROS
  )

  # 为 Breakpad .cc 源文件启用 C++11（部分文件需要）
  target_compile_features(breakpad_client PUBLIC cxx_std_11)

  # ── CrashTrace SO ────────────────────────────────────
  set(PLATFORM_SOURCES
    "platform/CrashTrace_android.h"
    "platform/CrashTrace_android.cpp"
  )

  add_library(CrashTrace SHARED ${COMMON_SOURCES} ${PLATFORM_SOURCES})
  target_include_directories(CrashTrace PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    "${BREAKPAD_ROOT}/src"                           # 访问 client/linux/handler/*.h
  )
  target_compile_definitions(CrashTrace PRIVATE CrashTrace_EXPORTS)

  # 链接 — Breakpad 静态库 + Android log
  target_link_libraries(CrashTrace PRIVATE breakpad_client log)

else()
  message(FATAL_ERROR "CrashTrace: unsupported platform (only Windows / Android)")
endif()

# ── 标准 ─────────────────────────────────────────────
if(CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET CrashTrace PROPERTY CXX_STANDARD 20)
endif()
