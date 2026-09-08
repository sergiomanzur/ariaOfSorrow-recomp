#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <filesystem>
#include "core/rom_validator.hpp"
#include "config/config_system.hpp"
#include "symbols/cvaos_symbols.hpp"
#include "runtime.h"

namespace {
void EnsureCleanroomBiosExists(const std::string& path) {
    std::filesystem::create_directories(std::filesystem::path(path).parent_path());
    std::vector<uint8_t> data(16384, 0);
    *reinterpret_cast<uint32_t*>(&data[0x00]) = 0xEA000006; // b 0x20
    *reinterpret_cast<uint32_t*>(&data[0x04]) = 0xEAFFFFFE; // b .
    *reinterpret_cast<uint32_t*>(&data[0x08]) = 0xEA000007; // b 0x2C (SWI)
    *reinterpret_cast<uint32_t*>(&data[0x0C]) = 0xEAFFFFFE; // b .
    *reinterpret_cast<uint32_t*>(&data[0x10]) = 0xEAFFFFFE; // b .
    *reinterpret_cast<uint32_t*>(&data[0x14]) = 0x00000000;
    *reinterpret_cast<uint32_t*>(&data[0x18]) = 0xEA000004; // b 0x30 (IRQ)
    *reinterpret_cast<uint32_t*>(&data[0x1C]) = 0xEAFFFFFE; // b .

    // Reset code -> branch to Cartridge Entry (0x08000000)
    *reinterpret_cast<uint32_t*>(&data[0x20]) = 0xE59F0000; // ldr r0, [pc] (0x28)
    *reinterpret_cast<uint32_t*>(&data[0x24]) = 0xE12FFF10; // bx r0
    *reinterpret_cast<uint32_t*>(&data[0x28]) = 0x08000000; // Cartridge entry

    // SWI return stub at 0x2C
    *reinterpret_cast<uint32_t*>(&data[0x2C]) = 0xE1B0F00E; // movs pc, lr

    // IRQ Handler at 0x30
    *reinterpret_cast<uint32_t*>(&data[0x30]) = 0xE92D500F; // push {r0-r3, r12, lr}
    *reinterpret_cast<uint32_t*>(&data[0x34]) = 0xE59F3010; // ldr r3, [pc, #16] -> loads 0x4C (0x03007FFC)
    *reinterpret_cast<uint32_t*>(&data[0x38]) = 0xE5933000; // ldr r3, [r3]
    *reinterpret_cast<uint32_t*>(&data[0x3C]) = 0xE1A0E00F; // mov lr, pc (LR = 0x44)
    *reinterpret_cast<uint32_t*>(&data[0x40]) = 0xE12FFF13; // bx r3 (jump to user handler)
    *reinterpret_cast<uint32_t*>(&data[0x44]) = 0xE8BD500F; // pop {r0-r3, r12, lr}
    *reinterpret_cast<uint32_t*>(&data[0x48]) = 0xE25EF004; // subs pc, lr, #4
    *reinterpret_cast<uint32_t*>(&data[0x4C]) = 0x03007FFC; // pointer literal 0x03007FFC

    std::ofstream out(path, std::ios::binary);
    out.write(reinterpret_cast<const char*>(data.data()), data.size());
}
} // namespace

#if defined(_WIN32)
#include <windows.h>
#include <commdlg.h>
#include <io.h>

namespace {
std::string PromptUserForRom(HWND hwndOwner = NULL) {
    char szFile[MAX_PATH] = "";
    OPENFILENAMEA ofn;
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hwndOwner;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile);
    ofn.lpstrFilter = "Game Boy Advance ROMs (*.gba)\0*.gba\0All Files (*.*)\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.lpstrTitle = "Select Castlevania: Aria of Sorrow (USA) GBA ROM";
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;

    if (GetOpenFileNameA(&ofn)) {
        return std::string(szFile);
    }
    return "";
}

void ShowErrorMessageBox(const std::string& title, const std::string& message) {
    MessageBoxA(NULL, message.c_str(), title.c_str(), MB_OK | MB_ICONERROR);
}
} // namespace
#else
namespace {
std::string PromptUserForRom() { return ""; }
void ShowErrorMessageBox(const std::string&, const std::string&) {}
} // namespace
#endif

namespace {
// Launcher state — the config, the cleanroom BIOS and its sidecar — belongs
// next to the executable, not in the working directory. A double-click from
// Explorer happens to set the cwd to the exe's own folder, which masks the
// difference entirely; a shortcut with "Start in" set, a Steam entry, or a
// launch from any other directory does not. Resolving against the cwd then
// fails to find the saved ROM path, re-prompts with the picker, and litters a
// fresh aria_config.ini + bios/ + bios.cfg wherever the process started.
std::filesystem::path ExecutableDir(const char* argv0) {
#if defined(_WIN32)
    std::wstring buf(MAX_PATH, L'\0');
    for (;;) {
        const DWORD n = GetModuleFileNameW(nullptr, buf.data(),
                                           static_cast<DWORD>(buf.size()));
        if (n == 0) break;                 // fall back to argv[0] below
        if (n < buf.size()) {
            buf.resize(n);
            return std::filesystem::path(buf).parent_path();
        }
        buf.resize(buf.size() * 2);        // path longer than the buffer
    }
#endif
    if (argv0 && *argv0) {
        const std::filesystem::path p(argv0);
        if (p.has_parent_path()) return p.parent_path();
    }
    return std::filesystem::current_path();
}

// Absolute form of a user-supplied path, for storing in the config. Falls back
// to the original spelling rather than losing the value outright.
std::string ToAbsolute(const std::string& path) {
    std::error_code ec;
    std::filesystem::path abs = std::filesystem::weakly_canonical(path, ec);
    if (ec) abs = std::filesystem::absolute(path, ec);
    return ec ? path : abs.string();
}
} // namespace

int main(int argc, char* argv[]) {
#if defined(_WIN32)
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut == NULL || hOut == INVALID_HANDLE_VALUE || GetFileType(hOut) == FILE_TYPE_UNKNOWN) {
        if (AttachConsole(ATTACH_PARENT_PROCESS)) {
            FILE* fp = nullptr;
            freopen_s(&fp, "CONOUT$", "w", stdout);
            freopen_s(&fp, "CONOUT$", "w", stderr);
            freopen_s(&fp, "CONIN$", "r", stdin);
        }
    }
    if (!std::getenv("GBARECOMP_HEAL_CXX")) {
        _putenv("GBARECOMP_HEAL_CXX=g++");
    }
#endif

    std::cout << "====================================================\n";
    std::cout << "  AriaRecomp - Modernized PC Port of Aria of Sorrow \n";
    std::cout << "====================================================\n";

    // 1. Load Centralized Configuration
    auto& configSystem = aria::config::ConfigSystem::Get();
    const std::filesystem::path exeDir = ExecutableDir(argv[0]);
    const std::string configPath = (exeDir / "aria_config.ini").string();
    if (std::filesystem::exists(configPath)) {
        configSystem.LoadFromFile(configPath);
        std::cout << "[INFO] Loaded configuration from " << configPath << "\n";
    } else {
        std::cout << "[INFO] Configuration file not found, creating default config.\n";
        configSystem.SaveToFile(configPath);
    }

    // 2. Ensure Cleanroom Non-Copyrighted BIOS Exists Automatically
    const std::string cleanroomBiosPath =
        (exeDir / "bios" / "cleanroom_bios.bin").string();
    EnsureCleanroomBiosExists(cleanroomBiosPath);

    // Write sidecar bios.cfg so GBARecomp never prompts the user for BIOS
    {
        std::ofstream bcfg((exeDir / "bios.cfg").string());
        if (bcfg.is_open()) {
            bcfg << cleanroomBiosPath << "\n";
        }
    }

    std::string romPath = configSystem.GetConfig().paths.romPath;
    // A config written by an older build may hold a bare filename, which only
    // resolves when the cwd happens to be the install directory. Re-anchor it
    // so an existing install keeps working from any launcher instead of
    // prompting once more; the save below then upgrades it to absolute.
    if (!romPath.empty() && std::filesystem::path(romPath).is_relative() &&
        !std::filesystem::exists(romPath)) {
        const std::filesystem::path anchored = exeDir / romPath;
        if (std::filesystem::exists(anchored)) romPath = anchored.string();
    }
    std::string biosPath = cleanroomBiosPath;

    bool romSpecifiedViaCli = false;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--rom" && i + 1 < argc) {
            romPath = argv[++i];
            romSpecifiedViaCli = true;
        } else if (arg == "--bios" && i + 1 < argc) {
            biosPath = argv[++i];
        } else if (!arg.empty() && arg[0] != '-') {
            if (arg.ends_with(".gba") || arg.ends_with(".GBA")) {
                romPath = arg;
                romSpecifiedViaCli = true;
            }
        }
    }

    // Default fallback if not specified via CLI or config
    if (romPath.empty() || !std::filesystem::exists(romPath)) {
        const std::filesystem::path bundled =
            exeDir / "Castlevania - Aria of Sorrow (USA).gba";
        if (std::filesystem::exists(bundled)) {
            romPath = bundled.string();
        }
    }

    // If still not found, prompt user with file dialog (for double-click workflow)
    if (!std::filesystem::exists(romPath)) {
        std::cout << "[INFO] ROM not found. Prompting user to select Castlevania: Aria of Sorrow ROM...\n";
        romPath = PromptUserForRom();
        if (romPath.empty()) {
            ShowErrorMessageBox(
                "AriaRecomp - ROM Required",
                "No ROM file was selected.\n\nAriaRecomp requires a legally dumped ROM of Castlevania: Aria of Sorrow (USA, Game Boy Advance) to run."
            );
            return 1;
        }
    }

    std::cout << "[INFO] Validating ROM: " << romPath << "\n";
    auto validation = aria::core::RomValidator::ValidateFile(romPath);
    if (!validation.isValid) {
        std::cerr << "[ERROR] " << validation.errorMessage << "\n";
        std::cerr << "[FATAL] AriaRecomp requires a verified USA ROM to proceed.\n";
        ShowErrorMessageBox("AriaRecomp - Invalid ROM", validation.errorMessage);
        return 1;
    }

    // Save verified ROM path to config for future double-click launches
    // Always store the absolute path: a bare filename resolves against the
    // working directory, so it would stop resolving the moment the game is
    // launched from anywhere else — the very case this anchoring exists for.
    if (!romSpecifiedViaCli) {
        configSystem.GetConfig().paths.romPath = ToAbsolute(romPath);
        configSystem.SaveToFile(configPath);
    }

    std::cout << "[SUCCESS] ROM verified successfully!\n";
    std::cout << "          Title:     " << validation.title << "\n";
    std::cout << "          Game Code: " << validation.gameCode << "\n";
    std::cout << "          SHA-1:     " << validation.sha1 << "\n";
    std::cout << "          MD5:       " << validation.md5 << "\n";
    std::cout << "          Size:      " << validation.sizeBytes << " bytes\n";

    std::cout << "\n[INFO] Starting Recompiled Faithful Runtime...\n";
    std::cout << "       Display Resolution: " << configSystem.GetConfig().display.windowWidth << "x"
              << configSystem.GetConfig().display.windowHeight << "\n";
    std::cout << "       Target Refresh:     " << configSystem.GetConfig().display.targetRefreshRate << " Hz\n";
    std::cout << "       Rewind Buffer:      " << configSystem.GetConfig().rewind.bufferDurationSeconds << " seconds\n";

    // 3. Configure GBARecomp Runtime
    gbarecomp::RunOptions opts;
    opts.builtin_game_name = "Castlevania: Aria of Sorrow (USA)";
    opts.builtin_rom_sha1 = aria::core::RomValidator::USA_SHA1;
    opts.mod_game_id = "cvaos_us";
    opts.max_view_width = 384;
    opts.freely_resizable_window = true;
    opts.show_fps_by_default = configSystem.GetConfig().developer.showFps;
    opts.expose_assist_tools = true;
    opts.save_state_slot_count = 10;
    opts.rewind_history_seconds = configSystem.GetConfig().rewind.enableRewind
        ? static_cast<uint8_t>(configSystem.GetConfig().rewind.bufferDurationSeconds)
        : 0;
    opts.rewind_capture_interval_frames = 1;

    // Build argument list for GBARecomp
    std::vector<std::string> args;
    args.push_back(argv[0]);
    args.push_back("--rom");
    args.push_back(romPath);

    const std::filesystem::path gameToml = exeDir / "game.toml";
    if (std::filesystem::exists(gameToml)) {
        args.push_back("--config");
        args.push_back(gameToml.string());
    }

    // Always provide the cleanroom BIOS and enable cleanroom HLE SWIs
    args.push_back("--bios");
    args.push_back(biosPath);
    args.push_back("--bios-hle");
    args.push_back("--bios-skip-intro");

    if (configSystem.GetConfig().display.fullscreen) {
        args.push_back("--fullscreen");
    }
    if (configSystem.GetConfig().display.aspectRatio == aria::config::AspectRatioMode::Widescreen_16_9) {
        args.push_back("--view-width");
        args.push_back("384");
    }

    // Pass through any other command line flags (e.g. --frames, --no-window, --scale, --bios-hle)
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--rom" || arg == "--bios") {
            ++i; // skip argument value
            continue;
        }
        if (arg == romPath || arg == biosPath) continue;
        args.push_back(arg);
    }

    std::vector<char*> cArgs;
    for (auto& s : args) {
        cArgs.push_back(s.data());
    }

    return gbarecomp::run_game(static_cast<int>(cArgs.size()), cArgs.data(), opts);
}
