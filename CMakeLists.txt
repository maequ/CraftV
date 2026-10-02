# RedCraft native code: the shared protocol library, its tests, the mock host, and (Phase 2) the
# RDR2 ASI plugin. Build with scripts/build-native.ps1, or:
#   cmake -S . -B build -G "Visual Studio 17 2022" -A x64
#   cmake --build build --config Release
cmake_minimum_required(VERSION 3.24)
project(RedCraft LANGUAGES CXX)

if(NOT WIN32)
	message(FATAL_ERROR "RedCraft's native parts are Windows-only (shared memory via Win32, RDR2 via ScriptHookRDR2).")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
# Static CRT: the ASI and the mock host run without a VC++ redistributable dependency.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

add_compile_options(/W4 /permissive- /utf-8 /EHsc /Zc:__cplusplus)
add_compile_definitions(UNICODE _UNICODE)

enable_testing()

add_subdirectory(protocol/cpp)
add_subdirectory(tools/mockhost)
add_subdirectory(rdr2)
