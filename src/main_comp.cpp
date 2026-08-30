#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>

#include "WaylandComp.hpp"
#include "config.hpp"
#include "wconf.hpp"

static void setup_binds() {
    auto& binds = wconf::get().binds;
    binds.add(wconf::get().keys.mod_logo, 36, wconf::Action::Spawn);  // Mod+Enter
    binds.add(wconf::get().keys.mod_logo | wconf::get().keys.mod_shift, 41, wconf::Action::ToggleFullscreen);  // Mod+Shift+F
    binds.add(wconf::get().keys.mod_logo, 24, wconf::Action::Quit);  // Mod+Q
    binds.add(wconf::get().keys.mod_logo, 9,  wconf::Action::Quit);  // Mod+Esc
    binds.add(wconf::get().keys.mod_logo, 28, wconf::Action::ToggleFloating);  // Mod+T
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
