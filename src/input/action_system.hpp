#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <functional>
#include "config/config_system.hpp"

namespace aria::input {

enum class Action : uint32_t {
    None = 0,
    // GBA Standard Actions
    Attack,         // GBA B button (default)
    Jump,           // GBA A button (default)
    Soul,           // GBA R button (Guardian/Bullet soul)
    Backdash,       // GBA L button
    DPadUp,         // GBA Up
    DPadDown,       // GBA Down
    DPadLeft,       // GBA Left
    DPadRight,      // GBA Right
    Start,          // GBA Start (Pause / Menu)
    Select,         // GBA Select (Map)

    // Modern Hotkeys
    QuickSave,
    QuickLoad,
    Rewind,
    Turbo,
    ToggleMenu,
    ToggleFullscreen,
    ToggleMute,

    Count
};

// GBA KEYINPUT bitmask constants (active-low: 0 = pressed, 1 = released)
constexpr uint16_t GBA_KEY_A      = (1 << 0);
constexpr uint16_t GBA_KEY_B      = (1 << 1);
constexpr uint16_t GBA_KEY_SELECT = (1 << 2);
constexpr uint16_t GBA_KEY_START  = (1 << 3);
constexpr uint16_t GBA_KEY_RIGHT  = (1 << 4);
constexpr uint16_t GBA_KEY_LEFT   = (1 << 5);
constexpr uint16_t GBA_KEY_UP     = (1 << 6);
constexpr uint16_t GBA_KEY_DOWN   = (1 << 7);
constexpr uint16_t GBA_KEY_R      = (1 << 8);
constexpr uint16_t GBA_KEY_L      = (1 << 9);
constexpr uint16_t GBA_KEY_MASK   = 0x03FF;

class ActionSystem {
public:
    static ActionSystem& Get();

    void Initialize(const aria::config::ControlsConfig& config);
    void Update();

    // Action State Queries
    bool IsActionHeld(Action action) const;
    bool IsActionJustPressed(Action action) const;
    bool IsActionJustReleased(Action action) const;

    // Direct GBA Keyinput Generation
    uint16_t GetGbaKeyinput() const;

    // Action Simulation (for testing, scripts, and macro replay)
    void SetActionState(Action action, bool pressed);
    void ClearAllActions();

    // Key & Button Mapping
    void BindKey(int keycode, Action action);
    void BindGamepadButton(int buttonIndex, Action action);
    void UnbindKey(int keycode);
    void UnbindGamepadButton(int buttonIndex);

    // Hotkey Callbacks
    using HotkeyCallback = std::function<void()>;
    void RegisterHotkeyCallback(Action action, HotkeyCallback callback);

private:
    ActionSystem();
    ~ActionSystem() = default;

    uint32_t m_currentActionMask = 0;
    uint32_t m_previousActionMask = 0;

    std::unordered_map<int, Action> m_keyBindings;
    std::unordered_map<int, Action> m_gamepadBindings;
    std::unordered_map<Action, HotkeyCallback> m_hotkeyCallbacks;
};

} // namespace aria::input
