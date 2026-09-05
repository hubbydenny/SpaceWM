#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>

#include "WaylandComp.hpp"
#include "config.hpp"
#include "wconf.hpp"

#include <xkbcommon/xkbcommon-keysyms.h>

static void setup_binds() {
    auto& binds = wconf::get().binds;
    auto& keys  = wconf::get().keys;

    // Mod+Shift+Enter -> Spawn terminal
    binds.add(keys.mod_logo | keys.mod_shift, 36, wconf::Action::Spawn, "", XKB_KEY_Return);
    // Mod+Shift+F -> Fullscreen
    binds.add(keys.mod_logo | keys.mod_shift, 41, wconf::Action::ToggleFullscreen, "", XKB_KEY_F);
    binds.add(keys.mod_logo | keys.mod_shift, 41, wconf::Action::ToggleFullscreen, "", XKB_KEY_f);
    // Mod+Q -> Close window
    binds.add(keys.mod_logo, 24, wconf::Action::Close, "", XKB_KEY_Q);
    binds.add(keys.mod_logo, 24, wconf::Action::Close, "", XKB_KEY_q);
    // Mod+Shift+Q -> Quit compositor
    binds.add(keys.mod_logo | keys.mod_shift, 24, wconf::Action::Quit, "", XKB_KEY_Q);
    binds.add(keys.mod_logo | keys.mod_shift, 24, wconf::Action::Quit, "", XKB_KEY_q);
    // Mod+Esc -> Quit compositor
    binds.add(keys.mod_logo, 9,  wconf::Action::Quit, "", XKB_KEY_Escape);
    // Mod+T -> Toggle floating
    binds.add(keys.mod_logo, 28, wconf::Action::ToggleFloating, "", XKB_KEY_T);
    binds.add(keys.mod_logo, 28, wconf::Action::ToggleFloating, "", XKB_KEY_t);
}

int main() {
    setup_binds();

    Compositor comp;
    if (!comp.init()) {
        std::cerr << "[ERROR] compositor init failed\n";
        return 1;
    }
    comp.run();
    return 0;
}
