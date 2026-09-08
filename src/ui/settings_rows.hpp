#pragma once

#include <cstddef>
#include <string>

// The in-game settings overlay's game-owned row catalog: Cheats and Quality
// of Life sections, appended to the engine's own Display/Graphics/Audio/
// System/Assist Tools catalog via RunOptions::ui_extra_items. Every key here
// is namespaced ("cheats.*", "gameplay.*") so it can never collide with the
// engine's own "display.*"/"graphics.*"/"assist.*" keys.
//
// Only rows backed by a real, working implementation are ever enabled (see
// AriaUiEnabled) -- a row with no implementation behind it reports disabled
// rather than presenting a toggle that silently does nothing.
namespace aria::ui {

// main.cpp computes the config path anchored to the executable's own
// directory (not the working directory -- see ExecutableDir there) before
// any of this module's callbacks can run. Call once, before opts.ui_set is
// ever reachable, so a row edit saves to the same file main.cpp loaded.
void SetConfigSavePath(std::string path);

// The item array recomp-ui reads from (RunOptions::ui_extra_items /
// ui_extra_item_count). Declared here as void* + count, matching
// RunOptions' own untyped field, so this header does not need recomp-ui's
// headers to be included wherever RunOptions is populated.
const void* AriaSettingsItems();
std::size_t AriaSettingsItemCount();

// RunOptions::ui_get / ui_set / ui_action / ui_enabled implementations.
int AriaUiGet(const char* key, int* valueOut);
int AriaUiSet(const char* key, int value);
int AriaUiAction(const char* key);
int AriaUiEnabled(const char* key);

} // namespace aria::ui
