# Native PC build (x86-64) of Super Mario Galaxy.
#
# This is separate from the decomp build (configure.py + ninja), which keeps producing the matching
# Wii binary. PC-specific code is guarded with TARGET_PC and the original code is kept.
# The GameCube/Wii SDK (GX, OS, VI, DVD, PAD, MTX) is provided by Aurora (extern/aurora).
# pc/include holds shims that map the Wii SDK header layout (revolution/*) onto Aurora's
# (dolphin/*); the Wii-only SDK parts (WPAD, KPAD, NAND, SC, AX) are implemented in pc/src.
# The layout follows a Wind Waker decomp port and Dusklight (TP's PC port).
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

# Local fixes to the pinned Aurora commit, kept as patches in pc/patches/aurora, applied in order
# once each. The applied ones are listed in extern/aurora/.smg_applied_patches (a later patch can
# change the context of an earlier one, so "reverse-applies" only identifies the last ones).
find_package(Git REQUIRED)
file(GLOB SMG_AURORA_PATCHES CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/pc/patches/aurora/*.patch)
list(SORT SMG_AURORA_PATCHES)
set(SMG_AURORA_STAMP ${CMAKE_SOURCE_DIR}/extern/aurora/.smg_applied_patches)
set(SMG_AURORA_APPLIED "")
if (EXISTS ${SMG_AURORA_STAMP})
    file(STRINGS ${SMG_AURORA_STAMP} SMG_AURORA_APPLIED)
endif ()
foreach (patch IN LISTS SMG_AURORA_PATCHES)
    get_filename_component(patch_name ${patch} NAME)
    if (patch_name IN_LIST SMG_AURORA_APPLIED)
        continue()
    endif ()
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
    file(APPEND ${SMG_AURORA_STAMP} "${patch_name}\n")
endforeach ()

# Aurora (and fmt) need /utf-8. The game doesn't get it: see smg_game_headers (Shift-JIS strings).
# fmt would otherwise push /utf-8 onto every target linking it (FMT_UNICODE adds a PUBLIC option).
set(FMT_UNICODE OFF CACHE BOOL "" FORCE)
set(SMG_SAVED_C_FLAGS "${CMAKE_C_FLAGS}")
set(SMG_SAVED_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
if (MSVC)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /utf-8")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /utf-8")
endif ()
add_subdirectory(extern/aurora EXCLUDE_FROM_ALL)
set(CMAKE_C_FLAGS "${SMG_SAVED_C_FLAGS}")
set(CMAKE_CXX_FLAGS "${SMG_SAVED_CXX_FLAGS}")
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
        ${CMAKE_BINARY_DIR}/generated/include
)
target_link_libraries(smg_game_headers INTERFACE aurora::core)
if (CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
    # /permissive and /Zc:strictStrings-: the decomp code passes string literals as char* and relies
    # on other pre-C++11 leniencies.
    # The sources are UTF-8; narrow string literals must be Shift-JIS like the game's data (the
    # Wii build converts them with sjiswrap): Japanese demo/effect/nerve names are looked up by
    # name or hash in the game files.
    target_compile_options(smg_game_headers INTERFACE /source-charset:utf-8 /execution-charset:.932)
    target_compile_options(smg_game_headers INTERFACE /FIpc_compat.h
            $<$<COMPILE_LANGUAGE:CXX>:/permissive> $<$<COMPILE_LANGUAGE:CXX>:/Zc:strictStrings->
            # Pointer truncation is a real bug on 64-bit; make it an error.
            /we4311 /we4302 /we4312
            # delete of an incomplete type skips the destructor and the array cookie (heap corruption).
            /we4150
            # Pointers to members: one general representation everywhere. With the default /vmb, a
            # pointer-to-member type formed while its class is only forward-declared gets a different
            # layout than in TUs where the class is complete, and calls through it jump anywhere.
            /vmg)
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
        LiveActor Map MapObj NPC NWC24 NameObj Player RhythmLib Ride Scene Screen Speaker System Util)
set(SMG_PC_LIBS ${SMG_JSYSTEM_LIBS} ${SMG_NW4R_LIBS} ${SMG_GAME_LIBS})
foreach (lib IN LISTS SMG_PC_LIBS)
    smg_add_lib(smg_${lib} ${LIB_${lib}_FILES})
    # Game code section (pc/include/pc_compat.h).
    target_compile_definitions(smg_${lib} PRIVATE SMGPC_GAME_CODE=1)
endforeach ()

# THPDraw.c uses Aurora's GD helpers, which are C++-only.
set_source_files_properties(src/Game/Screen/THPDraw.c PROPERTIES LANGUAGE CXX)

# PC platform layer: OS (threads, mutexes, alarms, messages), Wii services (Wii Remote via the
# gamepad, NAND saves, system settings, MEM2) and the Aurora bring-up.
smg_add_lib(smg_pc
        pc/src/platform.cpp
        pc/src/display.cpp
        pc/src/sdk_stubs.cpp
        pc/src/crash_handler.cpp
        pc/src/game_missing.cpp
        pc/src/rfl_stubs.cpp
        pc/src/audio/pc_audio.cpp
        pc/src/audio/pc_dsp.cpp
        pc/src/os/OSAlarm.cpp
        pc/src/os/OSContext.cpp
        pc/src/os/OSMutex.cpp
        pc/src/os/OSReport.cpp
        pc/src/os/OSThread.cpp
        pc/src/os/os_stubs.cpp
        pc/src/os/user_files.cpp
        pc/src/os/wii_arena.cpp
        pc/src/os/wii_system.cpp
        pc/src/input/wiimote.cpp
        pc/src/ui/xbox_glyphs.cpp
        pc/src/ui/xbox_glyphs_gdi.cpp
        pc/src/ui/window_icon.cpp
        pc/src/ui/settings_menu.cpp
        pc/src/fx/god_rays.cpp
        pc/src/fx/ssao.cpp)
if (WIN32)
    target_link_libraries(smg_pc PUBLIC dbghelp psapi)
endif ()
# pc/src/fx/ssao.cpp uses Aurora's gfx API (WebGPU types).
target_link_libraries(smg_pc PRIVATE dawn::webgpu_dawn)

# Data tables the decomp still links from the original binary (embedded BCSV tables), copied out
# of the user's main.dol at build time (never committed).
find_package(Python3 REQUIRED COMPONENTS Interpreter)
set(SMG_DOL ${CMAKE_SOURCE_DIR}/orig/RMGE01/sys/main.dol)
if (NOT EXISTS ${SMG_DOL})
    message(FATAL_ERROR "Put your disc's main.dol at ${SMG_DOL} (see README.md)")
endif ()
# The assets/*.hpp headers (config.yml `extract:`), written at configure time like the decomp build's.
execute_process(COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/pc/gen_assets.py ${SMG_DOL}
                ${CMAKE_SOURCE_DIR}/config/RMGE01/config.yml ${CMAKE_SOURCE_DIR}/config/RMGE01/symbols.txt
                ${CMAKE_BINARY_DIR}/generated/include
        COMMAND_ERROR_IS_FATAL ANY)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${SMG_DOL}
        ${CMAKE_SOURCE_DIR}/config/RMGE01/config.yml ${CMAKE_SOURCE_DIR}/tools/pc/gen_assets.py)
set(SMG_DOL_DATA ${CMAKE_BINARY_DIR}/generated/dol_data.cpp)
add_custom_command(OUTPUT ${SMG_DOL_DATA}
        COMMAND Python3::Interpreter ${CMAKE_SOURCE_DIR}/tools/pc/gen_dol_data.py ${SMG_DOL}
                ${CMAKE_SOURCE_DIR}/config/RMGE01/symbols.txt ${SMG_DOL_DATA}
        DEPENDS ${SMG_DOL} ${CMAKE_SOURCE_DIR}/tools/pc/gen_dol_data.py
        COMMENT "Extracting data tables from main.dol")
target_sources(smg_pc PRIVATE ${SMG_DOL_DATA})

# The game. The entry point is main() in src/Game/System/GameSystem.cpp (renamed aurora_main by
# aurora/main.h; Aurora provides the real process entry point).
set(SMG_GAME_TARGETS smg_pc)
foreach (lib IN LISTS SMG_PC_LIBS)
    list(APPEND SMG_GAME_TARGETS smg_${lib})
endforeach ()
add_executable(smgpc pc/src/game_main_stub.cpp)
target_link_libraries(smgpc PRIVATE smg_game_headers ${SMG_GAME_TARGETS}
        aurora::main aurora::gx aurora::gd aurora::si aurora::vi aurora::pad aurora::mtx
        aurora::os aurora::dvd aurora::thp)
if (MSVC)
    # The game libraries reference each other in every direction.
    target_link_options(smgpc PRIVATE /OPT:REF)
    # No identical-code folding: a host function folded into its twin in the game code section
    # would have its plain `new` sent to a JKRHeap (pc/include/pc_compat.h).
    target_link_options(smgpc PRIVATE /OPT:NOICF)
    # JKRHeap.cpp replaces the global operator new/delete on PC. Pull it in before the CRT's
    # versions get picked to resolve the first reference.
    target_link_options(smgpc PRIVATE
            "/INCLUDE:??2@YAPEAX_K@Z" "/INCLUDE:??3@YAXPEAX@Z" "/INCLUDE:??_U@YAPEAX_K@Z"
            "/INCLUDE:??_V@YAXPEAX@Z")
endif ()
# Executable icon, when it has been generated locally (tools/pc/make_icon.py; not committed).
if (WIN32 AND EXISTS ${CMAKE_SOURCE_DIR}/pc/res/smgpc.ico)
    target_sources(smgpc PRIVATE pc/smgpc.rc)
endif ()
aurora_copy_runtime_dlls(smgpc)
