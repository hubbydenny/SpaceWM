#pragma once

#include <cstdint>
#include <array>
#include <string>
#include <vector>

namespace wconf {

struct Border {
    int         width          = 3;
    int         inset_left     = 0;
    int         inset_right    = 0;
    int         inset_top      = 0;
    int         inset_bottom   = 0;
    std::array<float,4> color_focused   = {0.15f, 0.55f, 1.00f, 1.0f};
    std::array<float,4> color_unfocused = {0.20f, 0.20f, 0.20f, 1.0f};
    bool        enabled        = true;
};

struct Window {
    int         default_width  = 1280;
    int         default_height = 720;
    int         gap            = 8;
    int         min_width      = 100;
    int         min_height     = 80;
};

struct Tiling {
    int         padding        = 12;
    int         outer_gap      = 8;
    bool        auto_tile      = true;
};

struct Animations {
    int         focus_ms       = 100;
    int         move_ms        = 200;
    float       scale_min      = 0.95f;
    float       scale_max      = 1.0f;
};

struct Keybinds {
    const uint32_t    mod_logo       = 0x40;
    const uint32_t    mod_alt        = 0x08;
    const uint32_t    mod_shift      = 0x01;
    const uint32_t    mod_ctrl       = 0x04;
};

enum class Action {
    None,
    Quit,
    Spawn,
    ToggleFullscreen,
    Close,
    FocusNext,
    FocusPrev,
    ToggleFloating,
};

struct Spawn {
    std::string cmd;
};

struct Bind {
    uint32_t    mods   = 0;
    uint32_t    key    = 0;
    Action      action = Action::None;
    std::string arg;
};

struct Binds {
    std::vector<Bind> list;

    void add(uint32_t mods, uint32_t key, Action a, const std::string& arg = "") {
        list.push_back({mods, key, a, arg});
    }
};

struct Config {
    Border       border;
    Window       window;
    Tiling       tiling;
    Animations   anim;
    Binds        binds;
    Keybinds     keys;
    std::string  terminal      = "foot";
};

inline Config& get() {
    static Config c;
    return c;
}

}
