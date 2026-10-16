# Native PC build (x86-64) of Super Mario Galaxy.
#
# This is separate from the decomp build (configure.py + ninja), which keeps producing the matching
# Wii binary. PC-specific code is guarded with TARGET_PC and the original code is kept.
# The GameCube/Wii SDK (GX, OS, VI, DVD, PAD, MTX) is provided by Aurora (extern/aurora).
# pc/include holds shims that map the Wii SDK header layout (revolution/*) onto Aurora's
# (dolphin/*); the Wii-only SDK parts (WPAD, KPAD, NAND, SC, AX) are implemented in pc/src.
# The layout follows the Wind Waker port (D:\ZELDA_WINDWAKER_PC) and Dusklight (TP's PC port).
cmake_minimum_required(VERSION 3.25)

if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "RelWithDebInfo" CACHE STRING
            "Build type options: Debug Release RelWithDebInfo MinSizeRel" FORCE)
endif ()

project(smgpc LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if (CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
    add_compile_options(
            $<$<COMPILE_LANGUAGE:C,CXX>:/bigobj>
            $<$<COMPILE_LANGUAGE:C,CXX>:/MP>
            $<$<COMPILE_LANGUAGE:C,CXX>:/FS>
            $<$<COMPILE_LANGUAGE:C,CXX>:/utf-8>
            $<$<COMPILE_LANGUAGE:CXX>:/Zc:__cplusplus>
            $<$<COMPILE_LANGUAGE:C,CXX>:/W0>
            # Missing return values: UB the PowerPC build tolerated.
            $<$<COMPILE_LANGUAGE:C,CXX>:/we4715>
            $<$<COMPILE_LANGUAGE:C,CXX>:/we4716>
    )
endif ()

set(AURORA_ENABLE_DVD ON CACHE BOOL "" FORCE)
set(AURORA_ENABLE_CARD OFF CACHE BOOL "" FORCE)
set(AURORA_ENABLE_THP ON CACHE BOOL "" FORCE)
set(AURORA_ENABLE_RMLUI OFF CACHE BOOL "" FORCE)

# Local fixes to the pinned Aurora commit, kept as patches in pc/patches/aurora. Each is applied
# once (skipped when it already reverse-applies).
find_package(Git REQUIRED)
file(GLOB SMG_AURORA_PATCHES ${CMAKE_SOURCE_DIR}/pc/patches/aurora/*.patch)
foreach (patch IN LISTS SMG_AURORA_PATCHES)
    execute_process(COMMAND ${GIT_EXECUTABLE} apply --reverse --check --ignore-whitespace ${patch}
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/extern/aurora RESULT_VARIABLE already_applied
            OUTPUT_QUIET ERROR_QUIET)
    if (NOT already_applied EQUAL 0)
        execute_process(COMMAND ${GIT_EXECUTABLE} apply --ignore-whitespace ${patch}
                WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/extern/aurora RESULT_VARIABLE apply_failed)
        if (apply_failed)
            message(FATAL_ERROR "Failed to apply ${patch} to extern/aurora")
        endif ()
        message(STATUS "Applied ${patch}")
    endif ()
endforeach ()

add_subdirectory(extern/aurora EXCLUDE_FROM_ALL)
target_compile_definitions(aurora_mtx PRIVATE MTX_USE_PS=1)

# Headers and defines shared by every game target. VERSION/RMGx01 mirror configure.py
# (RMGE01 = 1, the user's disc).
add_library(smg_game_headers INTERFACE)
target_compile_definitions(smg_game_headers INTERFACE
        TARGET_PC=1 AVOID_UB=1 NDEBUG=1 MTX_USE_PS=1
        VERSION=1 RMGJ01=0 RMGE01=1 RMGP01=2 RMGK01=3)
# Order matters: the shims in pc/include and Aurora's SDK headers must win over the Wii SDK
# headers in libs/RVL_SDK/include. The MSL/Runtime/MetroTRK headers are not used: the host CRT
# replaces them.
target_include_directories(smg_game_headers INTERFACE
        ${CMAKE_SOURCE_DIR}/pc/include
        ${CMAKE_SOURCE_DIR}/extern/aurora/include
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/libs/JSystem/include
        ${CMAKE_SOURCE_DIR}/libs/nw4r/include
        ${CMAKE_SOURCE_DIR}/libs/RVLFaceLib/include
        ${CMAKE_SOURCE_DIR}/libs/RVL_SDK/include
        ${CMAKE_SOURCE_DIR}/build/RMGE01/include
)
target_link_libraries(smg_game_headers INTERFACE aurora::core)
if (CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
    # /permissive and /Zc:strictStrings-: the decomp code passes string literals as char* and relies
    # on other pre-C++11 leniencies.
    target_compile_options(smg_game_headers INTERFACE /FIpc_compat.h
            $<$<COMPILE_LANGUAGE:CXX>:/permissive> $<$<COMPILE_LANGUAGE:CXX>:/Zc:strictStrings->
            # Pointer truncation is a real bug on 64-bit; make it an error.
            /we4311 /we4302 /we4312)
else ()
    target_compile_options(smg_game_headers INTERFACE "SHELL:-include pc_compat.h")
endif ()

include(files.cmake)

function(smg_add_lib name)
    add_library(${name} STATIC ${ARGN})
    target_link_libraries(${name} PUBLIC smg_game_headers)
endfunction()

# Libraries ported so far. Each one is a static library built from the lists in files.cmake.
set(SMG_JSYSTEM_LIBS JSupport JKernel JUtility JGadget JMath J2DGraph J3DGraphBase
        J3DGraphAnimator J3DGraphLoader JParticle JAudio2)
set(SMG_NW4R_LIBS libnw4r_ut libnw4r_db libnw4r_math libnw4r_lyt)
set(SMG_GAME_LIBS Animation AreaObj AudioLib Boss Camera Demo Effect Enemy GameAudio Gravity
        LiveActor Map MapObj NPC NameObj Player RhythmLib Ride Scene Screen Speaker System Util)
set(SMG_PC_LIBS ${SMG_JSYSTEM_LIBS} ${SMG_NW4R_LIBS} ${SMG_GAME_LIBS})
foreach (lib IN LISTS SMG_PC_LIBS)
    smg_add_lib(smg_${lib} ${LIB_${lib}_FILES})
endforeach ()

# THPDraw.c uses Aurora's GD helpers, which are C++-only.
set_source_files_properties(src/Game/Screen/THPDraw.c PROPERTIES LANGUAGE CXX)
