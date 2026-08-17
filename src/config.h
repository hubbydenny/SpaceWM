#ifndef CONFIG_H
#define CONFIG_H
#include <X11/keysym.h>
#include <X11/keysymdef.h>
//#include <string>
//#include <vector>

bool Startup = true;
bool autostart = false; //wm autostart
bool fullscreen = false;
const char* monitor = "Virtual-1";
const char* terminal = "xterm -fa 'Hack Nerd Font Mono' -u8";
const char* layoutCmd = "setxkbmap us,ru -option grp:alt_shift_toggle";
//const char* wallpapers-path = "";

static const bool titlewindow = false;
static const bool bar = false;
const static uint32_t ResizingValue[] = { 200, 300 };
const static uint32_t CanvasSize = 32000;
const static int ResizeEdge = 16;
const static int DragTopHeight = 30;

uint16_t windowHeight = 850;
uint16_t windowWidth = 1000;

bool infinity = true;
bool tiling = false;

#define MOD_SHIFT   (1 << 0)
#define MOD_LOCK    (1 << 1)
#define MOD_CONTROL (1 << 2)
#define MOD_ALT     (1 << 3)
#define MOD_NUM     (1 << 4)
#define MOD_SUPER   (1 << 6)
/* so  about other keys, if u want to use numbers u need for example use MOD SUPER, XK_k and write it to keybindings constant, also if u want number use XB_1 and other, if its hard to use ask ai how to change bind and it will help u */

struct KeyBinding {
    uint32_t mods;
    unsigned long keysym;
    const char* action;
};

const KeyBinding keybindings[] = {
    { MOD_ALT,             XK_Tab,    "focus_next" },
    { MOD_SUPER | MOD_ALT, XK_Tab,    "focus_prev" },
    { MOD_SUPER,             XK_Return, "spawn_terminal" },
    { MOD_SUPER,             XK_r,      "resize" },
    { MOD_SUPER,             XK_f,      "toggle_fullscreen" },
    { MOD_SUPER,             XK_q,      "close" },
};

#endif
/* if u want ot change fade or shadows or zoom speed and other this things edit vcompmgr */
