cmake_minimum_required(VERSION 3.20)
project(AriaRecomp VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# recomp-ui supplies the in-game settings overlay (ImGui). gbarecomp gates the
# whole surface on GBARECOMP_RUNTIME_UI_ROOT resolving to a checkout that has
# src/recomp_runtime_ui.h; without it every overlay code path compiles out and
# Esc simply quits. Point at the vendored copy when present so a plain
# `cmake -B build` gets the menu, while an external checkout can still override.
if(NOT GBARECOMP_RUNTIME_UI_ROOT AND
   EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/recomp-ui/src/recomp_runtime_ui.h")
    set(GBARECOMP_RUNTIME_UI_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/third_party/recomp-ui"
        CACHE PATH "recomp-ui checkout providing the in-game settings overlay")
endif()

# Add GBARecomp Core Runtime
add_subdirectory(third_party/gbarecomp EXCLUDE_FROM_ALL)

# Include directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/symbols
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/recomp_out
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/gbarecomp/src/armv4t
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/gbarecomp/src/gba
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/gbarecomp/src/runtime
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/gbarecomp/src/debug
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/gbarecomp/external/arm-recomp-core/profiles/armv4t_gba
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/gbarecomp/external/arm-recomp-core/include
)

# Core Runtime Library
add_library(aria_core STATIC
    src/core/rom_validator.cpp
    src/config/config_system.cpp
    src/input/action_system.cpp
    src/gameplay/cheat_system.cpp
    src/gameplay/qol_system.cpp
)

# Shards from Recompilation. The BIOS-side generated_bios/ sources are NOT
# globbed here: gbarecomp's own CMakeLists.txt already links them into
# gbarecomp_runtime, conditionally on bios_recompiled.cpp existing (it falls
# back to bios_dispatch_stub.cpp otherwise). Duplicating them into this target
# broke a fresh clone outright -- bios_dispatch_table.cpp is tracked upstream
# and unconditionally needs bios_recompiled.h, which only exists after running
# `gba_recompile --bios` locally, so an unconditional glob here failed to
# compile before that optional step was ever run.
file(GLOB ARIA_SHARDS
    "${CMAKE_CURRENT_SOURCE_DIR}/recomp_out/recompiled_*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/recomp_out/dispatch_table.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/recomp_out/symbol_map.cpp"
)

# ARIA_SHARDS is empty on a fresh clone: recomp_out/ is gitignored (it embeds
# a translation of this ROM's copyrighted machine code, so it can never be
# committed) and is produced by running gba_recompile against your own
# legally-dumped ROM -- see README.md's "Building" section for the exact
# command. aria_core and its unit tests need no ROM and stay buildable either
# way; only the game executable itself needs the recompiled shards.
if(ARIA_SHARDS)
add_library(aria_shards STATIC ${ARIA_SHARDS})

# Main Executable
add_executable(aria_recomp
    src/main.cpp
)

# In-game settings overlay. gbarecomp adds only the ImGui SDL_Renderer2 adapter
# to its runtime library; ImGui core, the SDL2 platform backend and recomp-ui's
# own overlay sources (recomp_runtime_ui.c, runtime_ui_imgui.cpp) are the game
# target's to provide, which is what this call does. recomp_ui.cmake is included
# here as well as inside gbarecomp because its RUI_* path variables are
# directory-scoped and would otherwise be empty at our call site.
#
# RECOMP_LAUNCHER comes along with the bundle and un-gates the pre-boot launcher
# block in a host main(). Ours (src/main.cpp) has no such block and never
# includes launcher_seam.h, so the define is inert and only the in-game overlay
# is built.
if(GBARECOMP_RUNTIME_UI_ROOT AND
   EXISTS "${GBARECOMP_RUNTIME_UI_ROOT}/recomp_ui.cmake")
    set(RECOMP_UI_ROOT "${GBARECOMP_RUNTIME_UI_ROOT}")
    include("${GBARECOMP_RUNTIME_UI_ROOT}/recomp_ui.cmake")
    recomp_target_launcher_ui(aria_recomp CONSOLE gba)
    # recomp_target_launcher_ui pins the target to C++17; main.cpp uses C++20
    # (std::string::ends_with), so restore the project standard afterwards.
    set_target_properties(aria_recomp PROPERTIES
        CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)
    # recomp-ui is written for C++17, where u8"" literals are const char*.
    # C++20 retypes them to const char8_t*, which its ImGui glyph tables reject.
    # Disabling char8_t restores the C++17 meaning; nothing under src/ uses u8
    # literals, so this is confined to how those literals type.
    if(MSVC)
        target_compile_options(aria_recomp PRIVATE /Zc:char8_t-)
    else()
        target_compile_options(aria_recomp PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-fno-char8_t>)
    endif()
endif()

# Increase stack size for recompiled call frames
if(WIN32 AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_link_options(aria_recomp PRIVATE -Wl,--stack,67108864)
elseif(MSVC)
    target_link_options(aria_recomp PRIVATE /STACK:67108864)
endif()

# Build as a GUI (not console) app. A console-subsystem exe spawns a real
# console window alongside the game window on every launch -- on Windows,
# closing that console sends CTRL_CLOSE_EVENT to the whole attached process
# and terminates it, so a player who (understandably) closes what looks like
# a stray debug window kills the game. src/main.cpp already calls
# AttachConsole(ATTACH_PARENT_PROCESS) and reopens stdout/stderr onto it, so a
# launch from an existing terminal still shows all diagnostic output; a plain
# double-click (no parent console to attach to) now shows only the game
# window, with no console window at all.
if(WIN32 AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_link_options(aria_recomp PRIVATE -mwindows)
elseif(MSVC)
    target_link_options(aria_recomp PRIVATE /SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup)
endif()

if(APPLE OR MSVC)
    target_link_libraries(aria_recomp PRIVATE
        aria_core
        aria_shards
        gbarecomp_runtime
        gbarecomp_armv4t
        gbarecomp_gba
        gbarecomp_debug
        gbarecomp_recompile_core
        gbarecomp_heal_gate
    )
else()
    target_link_libraries(aria_recomp PRIVATE
        -Wl,--start-group
        aria_core
        aria_shards
        gbarecomp_runtime
        gbarecomp_armv4t
        gbarecomp_gba
        gbarecomp_debug
        gbarecomp_recompile_core
        gbarecomp_heal_gate
        -Wl,--end-group
    )
endif()
endif() # ARIA_SHARDS
if(NOT ARIA_SHARDS)
    message(STATUS
        "AriaRecomp: recomp_out/ is empty -- aria_recomp will not be built. "
        "Run gba_recompile against your own ROM first (README.md, 'Building'). "
        "aria_core and its unit tests build and run without a ROM.")
endif()

# Enable testing
enable_testing()

# Unit Tests
add_executable(test_rom_validator tests/test_rom_validator.cpp)
target_link_libraries(test_rom_validator PRIVATE aria_core)
add_test(NAME RomValidatorTest COMMAND test_rom_validator)

add_executable(test_config tests/test_config.cpp)
target_link_libraries(test_config PRIVATE aria_core)
add_test(NAME ConfigSystemTest COMMAND test_config)

add_executable(test_action_system tests/test_action_system.cpp)
target_link_libraries(test_action_system PRIVATE aria_core)
add_test(NAME ActionSystemTest COMMAND test_action_system)

add_executable(test_cheats_qol tests/test_cheats_qol.cpp)
target_link_libraries(test_cheats_qol PRIVATE aria_core)
add_test(NAME CheatsQolTest COMMAND test_cheats_qol)
