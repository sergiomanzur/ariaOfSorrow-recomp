#include "input/action_system.hpp"
#include <iostream>

namespace aria::input {

ActionSystem& ActionSystem::Get() {
    static ActionSystem instance;
    return instance;
}

ActionSystem::ActionSystem() {
    // Default Keyboard Layout (Standard Modern ARPG / Metroidvania)
    // Z / J / Left Mouse -> Attack (GBA B)
    // X / K / Space -> Jump (GBA A)
    // C / I / Right Mouse -> Soul (GBA R)
    // Shift / L -> Backdash (GBA L)
    // Enter / Escape -> Start (GBA Start)
    // Tab / M -> Select (GBA Select)
    // Arrow Keys / WASD -> D-Pad
    m_keyBindings[1]  = Action::Attack;    // Primary Attack Keycode
    m_keyBindings[2]  = Action::Jump;      // Primary Jump Keycode
    m_keyBindings[3]  = Action::Soul;      // Primary Soul Keycode
    m_keyBindings[4]  = Action::Backdash;  // Primary Backdash Keycode
    m_keyBindings[5]  = Action::Start;     // Start Keycode
    m_keyBindings[6]  = Action::Select;    // Select Keycode
    m_keyBindings[7]  = Action::DPadUp;
    m_keyBindings[8]  = Action::DPadDown;
    m_keyBindings[9]  = Action::DPadLeft;
    m_keyBindings[10] = Action::DPadRight;

    // Hotkeys
    m_keyBindings[101] = Action::QuickSave;
    m_keyBindings[102] = Action::QuickLoad;
    m_keyBindings[103] = Action::Rewind;
    m_keyBindings[104] = Action::Turbo;
}

void ActionSystem::Initialize(const aria::config::ControlsConfig& config) {
    // Apply user-defined overrides from config if present
    for (const auto& [name, keycode] : config.keyboardBindings) {
        if (name == "Attack")        BindKey(keycode, Action::Attack);
        else if (name == "Jump")     BindKey(keycode, Action::Jump);
        else if (name == "Soul")     BindKey(keycode, Action::Soul);
        else if (name == "Backdash") BindKey(keycode, Action::Backdash);
        else if (name == "Start")    BindKey(keycode, Action::Start);
        else if (name == "Select")   BindKey(keycode, Action::Select);
        else if (name == "Up")       BindKey(keycode, Action::DPadUp);
        else if (name == "Down")     BindKey(keycode, Action::DPadDown);
        else if (name == "Left")     BindKey(keycode, Action::DPadLeft);
        else if (name == "Right")    BindKey(keycode, Action::DPadRight);
        else if (name == "QuickSave") BindKey(keycode, Action::QuickSave);
        else if (name == "QuickLoad") BindKey(keycode, Action::QuickLoad);
        else if (name == "Rewind")    BindKey(keycode, Action::Rewind);
    }
}

void ActionSystem::Update() {
    m_previousActionMask = m_currentActionMask;

    // Trigger registered hotkey callbacks for just-pressed actions
    for (const auto& [action, callback] : m_hotkeyCallbacks) {
        if (IsActionJustPressed(action) && callback) {
            callback();
        }
    }
}

bool ActionSystem::IsActionHeld(Action action) const {
    if (action == Action::None || action >= Action::Count) return false;
    return (m_currentActionMask & (1u << static_cast<uint32_t>(action))) != 0;
}

bool ActionSystem::IsActionJustPressed(Action action) const {
    if (action == Action::None || action >= Action::Count) return false;
    uint32_t mask = (1u << static_cast<uint32_t>(action));
    return (m_currentActionMask & mask) && !(m_previousActionMask & mask);
}

bool ActionSystem::IsActionJustReleased(Action action) const {
    if (action == Action::None || action >= Action::Count) return false;
    uint32_t mask = (1u << static_cast<uint32_t>(action));
    return !(m_currentActionMask & mask) && (m_previousActionMask & mask);
}

uint16_t ActionSystem::GetGbaKeyinput() const {
    // Active low: 1 = released, 0 = pressed
    uint16_t keyinput = GBA_KEY_MASK;

    if (IsActionHeld(Action::Jump))      keyinput &= ~GBA_KEY_A;
    if (IsActionHeld(Action::Attack))    keyinput &= ~GBA_KEY_B;
    if (IsActionHeld(Action::Select))    keyinput &= ~GBA_KEY_SELECT;
    if (IsActionHeld(Action::Start))     keyinput &= ~GBA_KEY_START;
    if (IsActionHeld(Action::DPadRight)) keyinput &= ~GBA_KEY_RIGHT;
    if (IsActionHeld(Action::DPadLeft))  keyinput &= ~GBA_KEY_LEFT;
    if (IsActionHeld(Action::DPadUp))    keyinput &= ~GBA_KEY_UP;
    if (IsActionHeld(Action::DPadDown))  keyinput &= ~GBA_KEY_DOWN;
    if (IsActionHeld(Action::Soul))      keyinput &= ~GBA_KEY_R;
    if (IsActionHeld(Action::Backdash))  keyinput &= ~GBA_KEY_L;

    return keyinput;
}

void ActionSystem::SetActionState(Action action, bool pressed) {
    if (action == Action::None || action >= Action::Count) return;
    uint32_t bit = (1u << static_cast<uint32_t>(action));
    if (pressed) {
        m_currentActionMask |= bit;
    } else {
        m_currentActionMask &= ~bit;
    }
}

void ActionSystem::ClearAllActions() {
    m_currentActionMask = 0;
    m_previousActionMask = 0;
}

void ActionSystem::BindKey(int keycode, Action action) {
    m_keyBindings[keycode] = action;
}

void ActionSystem::BindGamepadButton(int buttonIndex, Action action) {
    m_gamepadBindings[buttonIndex] = action;
}

void ActionSystem::UnbindKey(int keycode) {
    m_keyBindings.erase(keycode);
}

void ActionSystem::UnbindGamepadButton(int buttonIndex) {
    m_gamepadBindings.erase(buttonIndex);
}

void ActionSystem::RegisterHotkeyCallback(Action action, HotkeyCallback callback) {
    m_hotkeyCallbacks[action] = std::move(callback);
}

} // namespace aria::input
