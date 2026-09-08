#include <cassert>
#include <iostream>
#include "input/action_system.hpp"

using namespace aria::input;

int main() {
    std::cout << "[TEST] Running ActionSystem Unit Tests...\n";

    auto& actionSys = ActionSystem::Get();
    actionSys.ClearAllActions();

    // 1. Initially no buttons pressed -> KEYINPUT = 0x03FF
    assert(actionSys.GetGbaKeyinput() == GBA_KEY_MASK);
    assert(!actionSys.IsActionHeld(Action::Jump));
    assert(!actionSys.IsActionHeld(Action::Attack));

    // 2. Press Jump (A Button)
    actionSys.SetActionState(Action::Jump, true);
    actionSys.Update();
    assert(actionSys.IsActionHeld(Action::Jump));
    assert(actionSys.IsActionJustPressed(Action::Jump));
    assert((actionSys.GetGbaKeyinput() & GBA_KEY_A) == 0); // Active low

    // 3. Keep holding Jump
    actionSys.Update();
    assert(actionSys.IsActionHeld(Action::Jump));
    assert(!actionSys.IsActionJustPressed(Action::Jump)); // Not just pressed anymore

    // 4. Press Attack (B Button) while Jump is held
    actionSys.SetActionState(Action::Attack, true);
    actionSys.Update();
    assert(actionSys.IsActionHeld(Action::Jump));
    assert(actionSys.IsActionHeld(Action::Attack));
    assert((actionSys.GetGbaKeyinput() & GBA_KEY_A) == 0);
    assert((actionSys.GetGbaKeyinput() & GBA_KEY_B) == 0);

    // 5. Release Jump
    actionSys.SetActionState(Action::Jump, false);
    actionSys.Update();
    assert(!actionSys.IsActionHeld(Action::Jump));
    assert(actionSys.IsActionJustReleased(Action::Jump));
    assert((actionSys.GetGbaKeyinput() & GBA_KEY_A) != 0); // Released (1)
    assert((actionSys.GetGbaKeyinput() & GBA_KEY_B) == 0); // Attack still held

    // 6. Test Hotkey Trigger Callback
    bool hotkeyFired = false;
    actionSys.RegisterHotkeyCallback(Action::QuickSave, [&]() {
        hotkeyFired = true;
    });

    actionSys.SetActionState(Action::QuickSave, true);
    actionSys.Update();
    assert(hotkeyFired == true);

    actionSys.ClearAllActions();
    std::cout << "[SUCCESS] ActionSystem Unit Tests Passed!\n";
    return 0;
}
