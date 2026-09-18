#include <cstdio>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <filesystem>
#include "core/rom_validator.hpp"
#include "config/config_system.hpp"
#include "gameplay/cheat_system.hpp"
#include "gameplay/grant_system.hpp"
#include "gameplay/qol_system.hpp"
#include "symbols/cvaos_symbols.hpp"
#include "ui/settings_rows.hpp"
#include "runtime.h"
#include "runtime_arm.h"

#if defined(GBARECOMP_RUNTIME_UI)
#include <SDL.h>
#include "imgui.h"
#endif

#include <cmath>
#include "graphics/adaptive_widescreen.hpp"
#include "graphics/display_filters.hpp"
#include "graphics/hd_ui_system.hpp"
#include "graphics/hd_sprite_system.hpp"

// PPU widescreen-margin pillarbox policy & tilemap provider
extern "C" int g_ws_pillarbox;
extern "C" int g_ws_pillarbox_left;
extern "C" int g_ws_pillarbox_right;
extern "C" int (*g_ws_tilemap_provider)(int bg, int hw_x, int screen_y, uint16_t* out_entry);

static std::vector<uint8_t> g_romBuffer;
static uint64_t g_currentFrameCount = 0;
static uint8_t* g_activeEwram = nullptr;
static size_t g_activeEwramSize = 0;
static uint16_t g_savedPlayerLuck = 0;
static bool g_luckTemporarilyModified = false;

extern "C" int AriaTilemapProviderCallback(int bg, int hw_x, int screen_y, uint16_t* out_entry) {
    return aria::graphics::AdaptiveWidescreen::Get().ProvideTilemapEntry(bg, hw_x, screen_y, out_entry);
}

void AriaFramebufferPostProcess(std::uint8_t* rgb, int width, int height) {
    const auto& gfx = aria::config::ConfigSystem::Get().GetConfig().graphics;
    if (gfx.hdSprites && g_activeEwram && g_activeEwramSize >= 0x20000) {
        aria::graphics::HdSpriteSystem::Get().CompositeToFramebuffer(
            rgb, width, height, g_activeEwram, g_activeEwramSize, g_currentFrameCount);
    }
}

namespace {
// Logical render width for the 16:9 view.
constexpr int kView16x9Width = 284;

void AriaExtendedViewFrame(const gbarecomp::ExtendedViewFrameInfo* frame) {
    const auto& gfx = aria::config::ConfigSystem::Get().GetConfig().graphics;
    if (gfx.adaptiveWidescreen) {
        aria::graphics::AdaptiveWidescreen::Get().UpdateFrame(frame, g_activeEwram, g_activeEwramSize);
    } else {
        g_ws_pillarbox = 1;
        g_ws_pillarbox_left = 0;
        g_ws_pillarbox_right = 0;
    }
}

void AriaFunctionEntryHook(uint32_t entryPc) {
    if (entryPc == 0x080683BCu && g_activeEwram && g_activeEwramSize >= 0x20000) {
        auto& qol = aria::gameplay::QolSystem::Get();
        auto& cheats = aria::gameplay::CheatSystem::Get();
        uint32_t enemyPtr = g_cpu.R[0];
        if (enemyPtr >= 0x02000000 && enemyPtr < 0x02040000) {
            uint32_t offset = enemyPtr - 0x02000000;
            // Enemy ID is stored at offset 0x36 of the enemy entity struct
            uint8_t enemyId = g_activeEwram[offset + 0x36];
            qol.SetRecentTargetEnemy(enemyId);

            // EXP Multiplier cheat
            if (cheats.GetConfig().enableCheats && cheats.GetConfig().expMultiplier > 1.0f) {
                if (0x1328C + sizeof(uint32_t) <= g_activeEwramSize) {
                    uint32_t* expPtr = reinterpret_cast<uint32_t*>(g_activeEwram + 0x1328C);
                    uint16_t baseExp = 0;
                    if (!g_romBuffer.empty() && 0x0E9644 + enemyId * 36 + 0x12 <= g_romBuffer.size()) {
                        baseExp = *reinterpret_cast<const uint16_t*>(g_romBuffer.data() + 0x0E9644 + enemyId * 36 + 0x10);
                    }
                    if (baseExp > 0) {
                        uint32_t bonusExp = static_cast<uint32_t>(baseExp * (cheats.GetConfig().expMultiplier - 1.0f));
                        *expPtr += bonusExp;
                    }
                }
            }

            // Guaranteed Souls cheat & Soul Pity system
            bool guaranteeSoul = (cheats.GetConfig().enableCheats && cheats.GetConfig().guaranteedSouls) ||
                (qol.IsPitySystemEnabled() &&
                 qol.GetEnemyDryKills(enemyId) >= static_cast<uint32_t>(qol.GetConfig().soulPityThreshold));

            if (guaranteeSoul) {
                // RNG seed 0x0600 guarantees that (RandomNumberGenerator() >> 2) % r4 == 0, forcing soul drop
                *reinterpret_cast<uint32_t*>(g_activeEwram + 0x00008) = 0x0600;
                qol.ResetPityCounter(enemyId);
            } else if (qol.IsLuckFixEnabled()) {
                uint16_t* luckPtr = reinterpret_cast<uint16_t*>(
                    g_activeEwram + aria::gameplay::qol_offsets::kPlayerLuckStat);
                g_savedPlayerLuck = *luckPtr;
                g_luckTemporarilyModified = true;
                *luckPtr = static_cast<uint16_t>(g_savedPlayerLuck * 8 + 512);
                qol.RecordEnemyKill(enemyId, false);
            }
        }
    }

    if ((entryPc == 0x0800125Cu || entryPc == 0x0800148Cu) && g_activeEwram) {
        uint16_t tileIndex = static_cast<uint16_t>(g_cpu.R[0]);
        uint16_t rowOffset = static_cast<uint16_t>(g_cpu.R[1]);
        uint8_t tileInfo = static_cast<uint8_t>(g_cpu.R[2]);
        uint32_t strPtr = g_cpu.R[3];
        const char* str = nullptr;
        if (strPtr >= 0x02000000 && strPtr < 0x02040000 && (strPtr - 0x02000000) < g_activeEwramSize) {
            str = reinterpret_cast<const char*>(g_activeEwram + (strPtr - 0x02000000));
        } else if (strPtr >= 0x08000000 && strPtr < 0x0A000000 && !g_romBuffer.empty() && (strPtr - 0x08000000) < g_romBuffer.size()) {
            str = reinterpret_cast<const char*>(g_romBuffer.data() + (strPtr - 0x08000000));
        }
        if (str) {
            aria::graphics::HdUiSystem::Get().OnWriteString(tileIndex, rowOffset, tileInfo, str, g_currentFrameCount);
        }
    }
}

void AriaEwramFrameWrite(std::uint8_t* ewram, std::size_t ewramSize) {
    ++g_currentFrameCount;
    g_activeEwram = ewram;
    g_activeEwramSize = ewramSize;

    if (g_luckTemporarilyModified) {
        uint16_t* luckPtr = reinterpret_cast<uint16_t*>(
            ewram + aria::gameplay::qol_offsets::kPlayerLuckStat);
        *luckPtr = g_savedPlayerLuck;
        g_luckTemporarilyModified = false;
    }

    aria::gameplay::CheatSystem::Get().ApplyFrameCheats(
        ewram, ewramSize, nullptr, 0);
    // Runs on the guest thread at a frame boundary -- the only place a write
    // to guest memory is safe. The overlay's action callback merely queues a
    // grant; this is where it lands.
    aria::gameplay::GrantSystem::Get().ApplyPending(ewram, ewramSize);

    auto& qol = aria::gameplay::QolSystem::Get();
    qol.Update(ewram, ewramSize);

#if defined(GBARECOMP_RUNTIME_UI)
    // Check Keyboard Hotkeys
    static bool s_prevQKey = false;
    static bool s_prevMKey = false;
    const Uint8* ks = SDL_GetKeyboardState(nullptr);
    if (ks) {
        bool qKey = (ks[SDL_SCANCODE_Q] != 0);
        if (qKey && !s_prevQKey) {
            qol.CycleLoadout(ewram, ewramSize, 1);
        }
        s_prevQKey = qKey;

        bool mKey = (ks[SDL_SCANCODE_M] != 0);
        if (mKey && !s_prevMKey) {
            auto cfg = qol.GetConfig();
            cfg.transparentMiniMap = !cfg.transparentMiniMap;
            qol.SetConfig(cfg);
        }
        s_prevMKey = mKey;

        static bool s_prevHKey = false;
        bool hKey = (ks[SDL_SCANCODE_H] != 0);
        if (hKey && !s_prevHKey) {
            auto& gcfg = aria::config::ConfigSystem::Get().GetConfig().graphics;
            gcfg.hdSprites = !gcfg.hdSprites;
            std::cout << "[INFO] HD Sprites " << (gcfg.hdSprites ? "ENABLED" : "DISABLED") << "\n";
        }
        s_prevHKey = hKey;
    }

    // Check Controller Triggers (L2 / LT)
    static bool s_prevL2 = false;
    int numJoysticks = SDL_NumJoysticks();
    for (int i = 0; i < numJoysticks; ++i) {
        if (SDL_IsGameController(i)) {
            SDL_GameController* pad = SDL_GameControllerOpen(i);
            if (pad) {
                Sint16 lt = SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_TRIGGERLEFT);
                bool l2Down = (lt > 16000);
                if (l2Down && !s_prevL2) {
                    qol.CycleLoadout(ewram, ewramSize, 1);
                }
                s_prevL2 = l2Down;
                break;
            }
        }
    }
#endif

    // GBA In-Game Input Combo (L held + Select newly pressed)
    if (ewramSize >= 0x20) {
        uint16_t heldInput = *reinterpret_cast<const uint16_t*>(ewram + 0x00014);
        uint16_t newInput  = *reinterpret_cast<const uint16_t*>(ewram + 0x00016);
        // Bit 9: L, Bit 2: Select
        if ((heldInput & (1 << 9)) && (newInput & (1 << 2))) {
            qol.CycleLoadout(ewram, ewramSize, 1);
        }
    }
}

#if defined(GBARECOMP_RUNTIME_UI)
void AriaImGuiOverlayRender() {
    auto& qol = aria::gameplay::QolSystem::Get();
    ImGuiIO& io = ImGui::GetIO();
    float displayW = io.DisplaySize.x;
    float displayH = io.DisplaySize.y;
    if (displayW <= 0.0f || displayH <= 0.0f) return;

    // 1. Quick Loadout Preset Banner Notification
    if (qol.HasRecentLoadoutChange()) {
        float ratio = qol.GetNotificationRemainingRatio();
        float alpha = std::min(1.0f, ratio * 2.0f);
        ImGui::SetNextWindowPos(ImVec2(displayW * 0.5f, 20.0f), ImGuiCond_Always, ImVec2(0.5f, 0.0f));
        ImGui::SetNextWindowBgAlpha(0.85f * alpha);
        ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
                                 ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing |
                                 ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove;
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(1.0f, 0.8f, 0.2f, alpha));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, alpha));
        if (ImGui::Begin("##LoadoutNotification", nullptr, flags)) {
            ImGui::TextUnformatted(qol.GetActiveLoadoutNotification().c_str());
        }
        ImGui::End();
        ImGui::PopStyleColor(2);
    }

    // 2. Transparent Mini-Map Overlay
    if (qol.IsTransparentMiniMapEnabled() && g_activeEwram && g_activeEwramSize >= 0x20000) {
        int playerX = 0, playerY = 0;
        std::vector<aria::gameplay::MiniMapCell> cells;
        qol.QueryLocalMiniMap(g_activeEwram, g_activeEwramSize, playerX, playerY, cells);

        ImGui::SetNextWindowPos(ImVec2(displayW - 16.0f, 16.0f), ImGuiCond_Always, ImVec2(1.0f, 0.0f));
        ImGui::SetNextWindowBgAlpha(qol.GetConfig().miniMapOpacity);
        ImGuiWindowFlags mapFlags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
                                    ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing |
                                    ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove;
        if (ImGui::Begin("##MiniMapOverlay", nullptr, mapFlags)) {
            ImDrawList* drawList = ImGui::GetWindowDrawList();
            ImVec2 p0 = ImGui::GetCursorScreenPos();
            constexpr float kCellSize = 10.0f;
            constexpr float kCellSpacing = 1.0f;

            float mapWidth = 9 * (kCellSize + kCellSpacing);
            float mapHeight = 9 * (kCellSize + kCellSpacing);
            ImGui::Dummy(ImVec2(mapWidth, mapHeight));

            for (const auto& cell : cells) {
                int relX = cell.x - (playerX - 4);
                int relY = cell.y - (playerY - 4);
                if (relX < 0 || relX >= 9 || relY < 0 || relY >= 9) continue;

                ImVec2 cellMin(p0.x + relX * (kCellSize + kCellSpacing),
                               p0.y + relY * (kCellSize + kCellSpacing));
                ImVec2 cellMax(cellMin.x + kCellSize, cellMin.y + kCellSize);

                if (cell.visited) {
                    drawList->AddRectFilled(cellMin, cellMax, IM_COL32(40, 110, 220, 200), 2.0f);
                } else if (cell.revealed) {
                    drawList->AddRectFilled(cellMin, cellMax, IM_COL32(80, 80, 90, 160), 2.0f);
                } else {
                    drawList->AddRect(cellMin, cellMax, IM_COL32(50, 50, 60, 100), 2.0f);
                }

                if (cell.x == playerX && cell.y == playerY) {
                    ImVec2 center((cellMin.x + cellMax.x) * 0.5f, (cellMin.y + cellMax.y) * 0.5f);
                    drawList->AddCircleFilled(center, 3.5f, IM_COL32(255, 225, 30, 255));
                }
            }
        }
        ImGui::End();
    }

    // 3. Enemy Soul Indicator Badge
    if (qol.IsEnemySoulIndicatorsEnabled() && g_activeEwram && g_activeEwramSize >= 0x20000) {
        ImGui::SetNextWindowPos(ImVec2(16.0f, displayH - 16.0f), ImGuiCond_Always, ImVec2(0.0f, 1.0f));
        ImGui::SetNextWindowBgAlpha(0.75f);
        ImGuiWindowFlags soulFlags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
                                     ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing |
                                     ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove;
        if (ImGui::Begin("##SoulIndicatorOverlay", nullptr, soulFlags)) {
            if (qol.HasRecentEnemy()) {
                uint16_t enemyId = qol.GetRecentTargetEnemy();
                int soulCount = qol.GetEnemySoulCount(g_activeEwram, g_activeEwramSize, enemyId);
                std::string enemyName = qol.GetEnemyName(enemyId);
                ImGui::TextColored(ImVec4(0.8f, 0.4f, 1.0f, 1.0f), "Target: %s", enemyName.c_str());
                ImGui::SameLine();
                if (soulCount > 0) {
                    ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "[Soul: %d/9]", soulCount);
                } else {
                    ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "[Soul: 0/9 Uncollected]");
                }
            } else {
                int loadout = qol.GetActiveLoadoutIndex() + 1;
                ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.9f, 1.0f), "Loadout Preset %d [Q / L2 to Swap]", loadout);
            }
        }
        ImGui::End();
    }

    // 4. Authentic Physical Display Filter & HD Vector Typography
    auto& cfg = aria::config::ConfigSystem::Get().GetConfig();
    int baseW = (cfg.display.aspectRatio == aria::config::AspectRatioMode::Widescreen_16_9) ? kView16x9Width : 240;
    int baseH = 160;

    float scale = std::min(displayW / static_cast<float>(baseW), displayH / static_cast<float>(baseH));
    if (cfg.display.integerScaling) {
        scale = std::max(1.0f, std::floor(scale));
    }
    float vpW = baseW * scale;
    float vpH = baseH * scale;
    float vpX = (displayW - vpW) * 0.5f;
    float vpY = (displayH - vpH) * 0.5f;

    ImDrawList* fgDrawList = ImGui::GetForegroundDrawList();

    if (cfg.display.displayFilter != aria::config::DisplayFilter::None) {
        aria::graphics::DisplayFilterRenderer::Get().RenderFilter(
            fgDrawList, vpX, vpY, vpW, vpH, cfg.display.displayFilter, baseW, baseH);
    }

    if (cfg.graphics.hdFonts) {
        aria::graphics::HdUiSystem::Get().RenderOverlay(
            fgDrawList, vpX, vpY, vpW, vpH, baseW, baseH, g_currentFrameCount);
    }

    if (cfg.graphics.hdSprites && g_activeEwram && g_activeEwramSize >= 0x20000) {
        aria::graphics::HdSpriteSystem::Get().RenderOverlay(
            fgDrawList, vpX, vpY, vpW, vpH, baseW, baseH, g_currentFrameCount,
            g_activeEwram, g_activeEwramSize);
    }
}
#endif
} // namespace

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

bool AriaCustomTcpCommand(std::string_view req, std::string& out) {
    auto contains = [&](const char* tok) {
        return req.find(tok) != std::string_view::npos;
    };

    if (contains("\"get_game_state\"")) {
        if (!g_activeEwram || g_activeEwramSize < 0x20000) {
            out = "{\"ok\":false,\"error\":\"EWRAM not initialized\"}";
            return true;
        }
        uint8_t level = g_activeEwram[0x13279];
        int16_t hp = *reinterpret_cast<const int16_t*>(g_activeEwram + 0x1327A);
        int16_t mp = *reinterpret_cast<const int16_t*>(g_activeEwram + 0x1327C);
        uint16_t maxHp = *reinterpret_cast<const uint16_t*>(g_activeEwram + 0x1327E);
        uint16_t maxMp = *reinterpret_cast<const uint16_t*>(g_activeEwram + 0x13280);
        uint16_t str = *reinterpret_cast<const uint16_t*>(g_activeEwram + 0x13282);
        uint16_t con = *reinterpret_cast<const uint16_t*>(g_activeEwram + 0x13284);
        uint16_t intStat = *reinterpret_cast<const uint16_t*>(g_activeEwram + 0x13286);
        uint16_t lck = *reinterpret_cast<const uint16_t*>(g_activeEwram + 0x13288);
        uint32_t exp = *reinterpret_cast<const uint32_t*>(g_activeEwram + 0x1328C);
        uint32_t gold = *reinterpret_cast<const uint32_t*>(g_activeEwram + 0x13290);
        uint8_t weapon = g_activeEwram[0x13268];
        uint8_t redSoul = g_activeEwram[0x13269];
        uint8_t blueSoul = g_activeEwram[0x1326A];
        uint8_t yellowSoul = g_activeEwram[0x1326B];
        uint8_t armor = g_activeEwram[0x1326C];
        uint8_t accessory = g_activeEwram[0x1326D];
        uint16_t totalSouls = *reinterpret_cast<const uint16_t*>(g_activeEwram + 0x13264);
        bool invincibility = (*reinterpret_cast<const uint32_t*>(g_activeEwram + 0x13260) & 0x2000) != 0;

        int ownedWeapons = 0;
        for (size_t i = 0; i < 0x3B; ++i) {
            if (g_activeEwram[0x132B4 + i] > 0) ++ownedWeapons;
        }

        int ownedItems = 0;
        for (size_t i = 0; i < 0x20; ++i) {
            if (g_activeEwram[0x13294 + i] > 0) ++ownedItems;
        }

        int activeEnemies = 0;
        for (size_t i = 1; i < 0xE0; ++i) {
            uint32_t off = 0x004E4 + i * 0x84;
            if (off + 0x84 <= g_activeEwramSize) {
                uint32_t uf = *reinterpret_cast<const uint32_t*>(g_activeEwram + off);
                uint8_t eid = g_activeEwram[off + 0x36];
                if (uf != 0 && eid >= 1 && eid <= 120) ++activeEnemies;
            }
        }

        char buf[512];
        std::snprintf(buf, sizeof(buf),
            "{\"ok\":true,\"level\":%u,\"hp\":%d,\"mp\":%d,\"max_hp\":%u,\"max_mp\":%u,"
            "\"str\":%u,\"con\":%u,\"int\":%u,\"lck\":%u,\"exp\":%u,\"gold\":%u,"
            "\"weapon\":%u,\"red_soul\":%u,\"blue_soul\":%u,\"yellow_soul\":%u,"
            "\"armor\":%u,\"accessory\":%u,\"total_souls\":%u,\"invincibility\":%s,"
            "\"owned_weapons\":%d,\"owned_items\":%d,\"active_enemies\":%d}",
            level, hp, mp, maxHp, maxMp, str, con, intStat, lck, exp, gold,
            weapon, redSoul, blueSoul, yellowSoul, armor, accessory, totalSouls,
            invincibility ? "true" : "false", ownedWeapons, ownedItems, activeEnemies);
        out = buf;
        return true;
    }

    if (contains("\"trigger_grant\"")) {
        auto& gs = aria::gameplay::GrantSystem::Get();
        if (contains("\"bullet_souls\"")) gs.Request(aria::gameplay::GrantKind::BulletSouls);
        else if (contains("\"guardian_souls\"")) gs.Request(aria::gameplay::GrantKind::GuardianSouls);
        else if (contains("\"enchant_souls\"")) gs.Request(aria::gameplay::GrantKind::EnchantSouls);
        else if (contains("\"ability_souls\"")) gs.Request(aria::gameplay::GrantKind::AbilitySouls);
        else if (contains("\"weapons\"")) gs.Request(aria::gameplay::GrantKind::Weapons);
        else if (contains("\"armor\"")) gs.Request(aria::gameplay::GrantKind::ArmorAndAccessories);
        else if (contains("\"consumables\"")) gs.Request(aria::gameplay::GrantKind::Consumables);
        else if (contains("\"reveal_map\"")) gs.Request(aria::gameplay::GrantKind::RevealMap);
        else if (contains("\"level_up_one\"")) gs.Request(aria::gameplay::GrantKind::LevelUpOne);
        out = "{\"ok\":true}";
        return true;
    }

    if (contains("\"set_level\"")) {
        size_t pos = req.find("\"level\"");
        if (pos != std::string_view::npos) {
            size_t colon = req.find(':', pos);
            if (colon != std::string_view::npos) {
                int lvl = std::atoi(req.data() + colon + 1);
                if (lvl >= 1 && lvl <= 99) {
                    aria::gameplay::GrantSystem::Get().RequestSetLevel(lvl);
                    out = "{\"ok\":true,\"target_level\":" + std::to_string(lvl) + "}";
                    return true;
                }
            }
        }
        out = "{\"ok\":false,\"error\":\"invalid level\"}";
        return true;
    }

    if (contains("\"set_cheat\"")) {
        auto& cheats = aria::gameplay::CheatSystem::Get();
        auto cfg = cheats.GetConfig();
        cfg.enableCheats = true;
        if (contains("\"infinite_hp\"")) {
            cfg.infiniteHP = !contains("\"value\": false") && !contains("\"value\":false");
        }
        if (contains("\"infinite_mp\"")) {
            cfg.infiniteMP = !contains("\"value\": false") && !contains("\"value\":false");
        }
        if (contains("\"invincibility\"")) {
            cfg.invincibility = !contains("\"value\": false") && !contains("\"value\":false");
        }
        if (contains("\"one_hit_kill\"")) {
            cfg.oneHitKill = !contains("\"value\": false") && !contains("\"value\":false");
        }
        if (contains("\"guaranteed_souls\"")) {
            cfg.guaranteedSouls = !contains("\"value\": false") && !contains("\"value\":false");
        }
        if (contains("\"exp_multiplier\"")) {
            size_t pos = req.find("\"value\"");
            if (pos != std::string_view::npos) {
                size_t colon = req.find(':', pos);
                if (colon != std::string_view::npos) {
                    cfg.expMultiplier = static_cast<float>(std::atof(req.data() + colon + 1));
                }
            }
        }
        cheats.SetConfig(cfg);
        out = "{\"ok\":true}";
        return true;
    }

    if (contains("\"set_qol\"")) {
        auto& qol = aria::gameplay::QolSystem::Get();
        auto cfg = qol.GetConfig();
        if (contains("\"quick_loadouts\"")) {
            cfg.enableQuickLoadouts = !contains("\"value\": false") && !contains("\"value\":false");
        }
        if (contains("\"fix_luck\"")) {
            cfg.fixLuckStat = !contains("\"value\": false") && !contains("\"value\":false");
        }
        if (contains("\"soul_pity\"")) {
            cfg.farmPitySystem = !contains("\"value\": false") && !contains("\"value\":false");
        }
        if (contains("\"transparent_minimap\"")) {
            cfg.transparentMiniMap = !contains("\"value\": false") && !contains("\"value\":false");
        }
        if (contains("\"soul_indicators\"")) {
            cfg.enemySoulIndicators = !contains("\"value\": false") && !contains("\"value\":false");
        }
        if (contains("\"fast_doors\"")) {
            cfg.fastDoorTransitions = !contains("\"value\": false") && !contains("\"value\":false");
        }
        if (contains("\"fast_text\"")) {
            cfg.fastText = !contains("\"value\": false") && !contains("\"value\":false");
        }
        qol.SetConfig(cfg);
        out = "{\"ok\":true}";
        return true;
    }

    if (contains("\"set_graphics\"")) {
        auto& gfx = aria::config::ConfigSystem::Get().GetConfig().graphics;
        if (contains("\"hd_sprites\"")) {
            bool val = true;
            if (contains("\"hd_sprites\": false") || contains("\"hd_sprites\":false") ||
                contains("\"value\": false") || contains("\"value\":false")) {
                val = false;
            }
            gfx.hdSprites = val;
        }
        out = "{\"ok\":true,\"hd_sprites\":" + std::string(gfx.hdSprites ? "true" : "false") + "}";
        return true;
    }

    if (contains("\"cycle_loadout\"")) {
        if (g_activeEwram && g_activeEwramSize >= 0x20000) {
            aria::gameplay::QolSystem::Get().CycleLoadout(g_activeEwram, g_activeEwramSize, 1);
            out = "{\"ok\":true}";
            return true;
        }
        out = "{\"ok\":false,\"error\":\"EWRAM not ready\"}";
        return true;
    }

    return false;
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
    // 16:9 at the GBA's authentic 160 lines is 160*16/9 = 284.44 px wide.
    // 284 keeps the two margin columns symmetric (22 px each side), which
    // matters because they render as black bars: an odd width would make one
    // bar visibly wider than the other. This is also the ceiling, so a wider
    // request from any other source clamps here instead of going ultrawide --
    // it used to be 384, which is 2.4:1, not 16:9 at all.
    opts.max_view_width = kView16x9Width;
    opts.widescreen_view_width = kView16x9Width;
    opts.launcher_expose_widescreen = true;
    opts.freely_resizable_window = true;
    opts.show_fps_by_default = configSystem.GetConfig().developer.showFps;
    opts.expose_assist_tools = true;
    opts.save_state_slot_count = 10;
    opts.rewind_history_seconds = configSystem.GetConfig().rewind.enableRewind
        ? static_cast<uint8_t>(configSystem.GetConfig().rewind.bufferDurationSeconds)
        : 0;
    opts.rewind_capture_interval_frames = 1;
    aria::gameplay::QolSystem::Get().Initialize(configSystem.GetConfig().gameplay);
    aria::gameplay::CheatSystem::Get().Initialize(configSystem.GetConfig().cheats);

    // The map-reveal grant needs the ROM's room-occupancy table
    // (`sUnk_08116650`, ROM 0x08116650 == file offset 0x116650). Read it
    // straight from the ROM file here and cache it, rather than adding an
    // engine hook for ROM access: it is immutable data, the file is already
    // known-good (validated above), and the grant only ever needs it once.
    // The cells are little-endian u16 both on the GBA and on every host this
    // builds for, so the raw read needs no byte swapping.
    {
        std::vector<uint16_t> roomTable(aria::gameplay::grant_offsets::kRoomTableCells);
        std::ifstream rom(romPath, std::ios::binary);
        rom.seekg(aria::gameplay::grant_offsets::kRoomTableFileOffset);
        if (rom && rom.read(reinterpret_cast<char*>(roomTable.data()),
                            static_cast<std::streamsize>(roomTable.size() * sizeof(uint16_t)))) {
            aria::gameplay::GrantSystem::Get().SetRoomTable(roomTable.data(),
                                                            roomTable.size());
        } else {
            // Leaving the table unset disables only the map-reveal grant.
            std::cerr << "[WARN] could not read the room table from the ROM; "
                         "the reveal-map cheat will do nothing.\n";
        }
    }

    // The level-up cheats need the ROM's four growth tables (STR/CON/INT/MP
    // -- see grant_offsets in grant_system.hpp for their addresses and the
    // decomp citations behind them). Same reasoning as the room table just
    // above: immutable ROM data, the file is already validated, read once.
    {
        using aria::gameplay::grant_offsets::kGrowthTableSize;
        std::vector<uint8_t> strTable(kGrowthTableSize);
        std::vector<uint8_t> conTable(kGrowthTableSize);
        std::vector<uint8_t> intTable(kGrowthTableSize);
        std::vector<uint8_t> mpTable(kGrowthTableSize);

        auto readTable = [&romPath](uint32_t fileOffset, std::vector<uint8_t>& out) {
            std::ifstream rom(romPath, std::ios::binary);
            rom.seekg(fileOffset);
            return static_cast<bool>(
                rom && rom.read(reinterpret_cast<char*>(out.data()),
                                 static_cast<std::streamsize>(out.size())));
        };

        const bool ok =
            readTable(aria::gameplay::grant_offsets::kGrowthTableStrFileOffset, strTable) &&
            readTable(aria::gameplay::grant_offsets::kGrowthTableConFileOffset, conTable) &&
            readTable(aria::gameplay::grant_offsets::kGrowthTableIntFileOffset, intTable) &&
            readTable(aria::gameplay::grant_offsets::kGrowthTableMpFileOffset, mpTable);

        if (ok) {
            aria::gameplay::GrantSystem::Get().SetGrowthTables(
                strTable.data(), conTable.data(), intTable.data(), mpTable.data(),
                kGrowthTableSize);
        } else {
            // Leaving the tables unset disables only the level-up grants,
            // rather than proceeding with invented growth numbers.
            std::cerr << "[WARN] could not read the level-up growth tables "
                         "from the ROM; the level-up cheats will do nothing.\n";
        }
    }

    // Where the runtime keeps this ROM's battery save: the ROM path with its
    // extension replaced by .sav (third_party/gbarecomp/src/runtime/
    // runtime.cpp:1620-1623). The grant system copies it once before the
    // first grant lands, since a grant permanently edits a real save.
    {
        std::filesystem::path savePath(romPath);
        savePath.replace_extension(".sav");
        aria::gameplay::GrantSystem::Get().SetSavePath(savePath.string());
    }

    // The QoL enemy table for soul indicators and pity counter:
    {
        constexpr size_t kEnemyCount = 120;
        std::vector<uint8_t> enemyTable(kEnemyCount * aria::gameplay::qol_offsets::kEnemyEntrySize);
        std::ifstream rom(romPath, std::ios::binary);
        rom.seekg(aria::gameplay::qol_offsets::kEnemyTableFileOffset);
        if (rom && rom.read(reinterpret_cast<char*>(enemyTable.data()),
                            static_cast<std::streamsize>(enemyTable.size()))) {
            aria::gameplay::QolSystem::Get().SetEnemyTable(enemyTable.data(), enemyTable.size());
        }
    }

    // Load full ROM buffer for Adaptive Widescreen tile decoding
    {
        std::ifstream romFile(romPath, std::ios::binary);
        if (romFile) {
            romFile.seekg(0, std::ios::end);
            size_t sz = static_cast<size_t>(romFile.tellg());
            romFile.seekg(0, std::ios::beg);
            g_romBuffer.resize(sz);
            romFile.read(reinterpret_cast<char*>(g_romBuffer.data()), sz);
            aria::graphics::AdaptiveWidescreen::Get().SetRomData(g_romBuffer.data(), g_romBuffer.size());
        }
    }

    g_ws_tilemap_provider = &AriaTilemapProviderCallback;
    g_runtime_fn_entry_hook = AriaFunctionEntryHook;

    opts.extended_view_frame = AriaExtendedViewFrame;
    opts.ewram_frame_write = AriaEwramFrameWrite;
    opts.custom_tcp_cmd = AriaCustomTcpCommand;
    opts.framebuffer_post_process = AriaFramebufferPostProcess;
#if defined(GBARECOMP_RUNTIME_UI)
    opts.imgui_overlay_render = AriaImGuiOverlayRender;
    aria::ui::SetConfigSavePath(configPath);
    opts.ui_extra_items = aria::ui::AriaSettingsItems();
    opts.ui_extra_item_count = aria::ui::AriaSettingsItemCount();
    opts.ui_get = aria::ui::AriaUiGet;
    opts.ui_set = aria::ui::AriaUiSet;
    opts.ui_action = aria::ui::AriaUiAction;
    opts.ui_enabled = aria::ui::AriaUiEnabled;
#endif

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
        args.push_back(std::to_string(kView16x9Width));
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
