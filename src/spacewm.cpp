#include <iostream>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <xcb/xcb.h>
#include <chrono>
#include <cstdlib>
#include <cmath> 
#include <algorithm> //TODO make lerp animation
#include <array>
#include <vector>
#include <sys/sysinfo.h>
#include <xcb/xproto.h>
#include <xcb/xcb_cursor.h>
#include <csignal>
#include <cstring>
#include <string>
#include <xcb/xcb_atom.h>
#include <xcb/xcb_keysyms.h>
#include <X11/keysym.h>
#include <X11/cursorfont.h>
#include <xcb/randr.h>
#include "config.h"
#include <atomic>
#include <filesystem>
#include <fstream>

float lerp (float start, float end, float t) {
  return start + t * (end - start);
}
float startpos = 0.0f;
float endpos = 100.f;
float currentPosition = std::lerp(startpos, endpos, 0.5f); // will try make anim

namespace fs = std::filesystem;
class wmstarter {
private:
    std::string wmName = "spacewm";
    std::string systemBinaryPath = "/usr/local/bin/spacewm";
    std::string desktopFilePath = "/usr/share/xsessions/spacewm.desktop";
    std::string GetCurrentBinaryPath() {
        char buffer[1024];
        ssize_t len = readlink("/proc/self/exe", buffer, sizeof(buffer) - 1);
        if (len != -1) {
            buffer[len] = '\0';
            return std::string(buffer);
        }
        return "";
    }

public:
    void AutomateSetup() {
        std::string currentBin = GetCurrentBinaryPath();
        if (!autostart) return;
        if (fs::exists(desktopFilePath) && fs::exists(systemBinaryPath)) {
            return; 
        }
        
        std::string homeDir = std::getenv("HOME");
        std::string configDir = homeDir + "/.config/" + wmName;
        std::string autostartScript = configDir + "/autostart";

        if (!fs::exists(configDir)) {
            fs::create_directories(configDir);
            // template of autostart for bash
            std::ofstream script(autostartScript);
            script << "#!/bin/bash\n";
            script.close(); 
            fs::permissions(autostartScript, fs::perms::owner_all | fs::perms::group_read | fs::perms::others_read);
            std::cout << "[WM] template of autostart: " << autostartScript << "\n";
        }
        std::string installCmd = "pkexec sh -c '"; 
        installCmd += "cp \"" + currentBin + "\" \"" + systemBinaryPath + "\" && ";
        installCmd += "chmod 755 \"" + systemBinaryPath + "\" && ";
        
      //creating .desktop file 
        std::string tempDesktop = "/tmp/" + wmName + ".desktop";
        std::ofstream desktop(tempDesktop);
        desktop << "[Desktop Entry]\n"
                << "Name=MyWM\n"
                << "Comment=My Custom XCB Window Manager\n"
                << "Exec=" << systemBinaryPath << "\n"
                << "Type=Application\n"
                << "DesktopNames=MyWM\n";
        desktop.close();

        installCmd += "cp \"" + tempDesktop + "\" \"" + desktopFilePath + "\" && ";
        installCmd += "chmod 644 \"" + desktopFilePath + "\"'";

  
        int result = std::system(installCmd.c_str());

        if (result == 0) {
            std::cout << "[WM] success spacewm is in session\n";
        } else {
            std::cerr << "[WM] error 5\n";
        }
    }

    void SetupXinitrc() {
        if (!autostart) return;
        std::string homeDir = std::getenv("HOME");
        std::string xinitrcPath = homeDir + "/.xinitrc";
        std::string autostartScript = homeDir + "/.config/" + wmName + "/autostart"; 
        if (fs::exists(xinitrcPath)) {
            std::ifstream fileCheck(xinitrcPath);
            std::string line;
            while (std::getline(fileCheck, line)) {         
                if (line.find(systemBinaryPath) != std::string::npos || line.find(wmName) != std::string::npos) {
               
                    return;
                }
            }
        }

        std::cout << "[WM] setting up wm(.xinitrc)...\n";
        std::ofstream xinitrc(xinitrcPath, std::ios::app);
        
        if (!xinitrc.is_open()) {
            std::cerr << "[WM] error cant open or create xinirc\n";
            return;
        }

        xinitrc << "\n";
        xinitrc << "# --- setting for " << wmName << " ---\n";
        

        if (fs::exists(autostartScript)) {
            xinitrc << autostartScript << " &\n";
        }
        xinitrc << "exec " << systemBinaryPath << "\n";
        xinitrc.close(); 
        fs::permissions(xinitrcPath, fs::perms::owner_read | fs::perms::owner_write);
        std::cout << "[WM] success, reboot \n";
    }

    void SetupConsoleAutostart() {
        if (!autostart) return;
        std::string homeDir = std::getenv("HOME");
        std::string bashProfilePath = homeDir + "/.bash_profile";
        std::string marker = "spacewm-console-autostart";

        if (fs::exists(bashProfilePath)) {
            std::ifstream fileCheck(bashProfilePath);
            std::string line;
            while (std::getline(fileCheck, line)) {
                if (line.find(marker) != std::string::npos)
                    return;
            }
        }

        std::ofstream bashProfile(bashProfilePath, std::ios::app);
        if (!bashProfile.is_open()) {
            std::cerr << "[WM] error cant open or create .bash_profile\n";
            return;
        }

        bashProfile << "\n# " << marker << " - auto-start X on VT1 login\n";
        bashProfile << "if [[ -z $DISPLAY ]] && [[ ${XDG_VTNR:-0} == 1 ]]; then\n";
        bashProfile << "    exec startx\n";
        bashProfile << "fi\n";
        bashProfile.close();
        std::cout << "[WM] console autostart added to ~/.bash_profile\n";
    }
};

enum class Action : uint8_t {
    FocusNext,
    FocusPrev,
    Resize,
    SpawnTerminal,
    ToggleFullscreen,
    Close,
    SpawnApp,
    SpawnAtStartup
};

struct Binding {
    uint32_t modmask;
    xcb_keysym_t keysym;
    Action action;
};

static Action actionFromName(const char* name) {
    struct Map { const char* name; Action action; };
    static const Map map[] = {
        { "focus_next",        Action::FocusNext },
        { "focus_prev",        Action::FocusPrev },
        { "resize",            Action::Resize },
        { "spawn_terminal",    Action::SpawnTerminal },
        { "toggle_fullscreen", Action::ToggleFullscreen },
        { "close",             Action::Close },
        { "spawnapp", Action::SpawnApp },
        { "Spwnatstartup", Action::SpawnAtStartup },
    };
    for (const Map& m : map)
        if (std::strcmp(m.name, name) == 0) return m.action;
    return Action::SpawnTerminal;
}

static const size_t bindingCount = sizeof(keybindings) / sizeof(keybindings[0]);
static Binding bindings[bindingCount] = {};
static int initBindings() {
    for (size_t i = 0; i < bindingCount; ++i)
        bindings[i] = { keybindings[i].mods, (xcb_keysym_t)keybindings[i].keysym,
                        actionFromName(keybindings[i].action) };
    return 0;
}
static const int bindingsInitialized = initBindings();

static xcb_keycode_t keysymToKeycode(xcb_key_symbols_t* ks, xcb_keysym_t sym) {
    xcb_keycode_t* codes = xcb_key_symbols_get_keycode(ks, sym);
    if (!codes) return 0;
    xcb_keycode_t kc = codes[0];
    free(codes);
    return kc;
}

static void grabBindings(xcb_connection_t* conn, xcb_window_t root, xcb_key_symbols_t* ks) {
    const uint32_t lockMasks[] = {
        0,
        XCB_MOD_MASK_LOCK,
        XCB_MOD_MASK_2,
        XCB_MOD_MASK_LOCK | XCB_MOD_MASK_2,
    };
    for (const Binding& b : bindings) {
        xcb_keycode_t kc = keysymToKeycode(ks, b.keysym);
        if (!kc) {
            std::cerr << "[x] spacewm: no keycode for keysym 0x" << std::hex
                      << b.keysym << std::dec << std::endl;
            continue;
        }
        for (uint32_t lock : lockMasks)
            xcb_grab_key(conn, 1, root, b.modmask | lock, kc,
                         XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC);
    }
    xcb_flush(conn);
}
struct ClientRec {
    xcb_window_t win;
    int baseX, baseY, baseW, baseH;
    int saveX, saveY, saveW, saveH;
    bool isFullscreen = false;
};
static std::vector<ClientRec> clients;
static size_t focusedIndex = 0;
static int screenW = 0, screenH = 0;
static xcb_window_t moveWin = 0;
static int moveRX = 0, moveRY = 0, moveBX = 0, moveBY = 0;
static int pressX = 0, pressY = 0;
static bool clickPending = false;
static xcb_window_t rootWin = 0;
static xcb_atom_t wm_name_atom = 0;
static xcb_atom_t wm_icon_name_atom = 0;
static xcb_atom_t net_wm_state_atom = 0;
static xcb_atom_t net_wm_state_fullscreen_atom = 0;
static xcb_atom_t wm_protocols_atom = 0;
static xcb_atom_t wm_delete_window_atom = 0;
const char* title = "Hi";
const char* title_icon = "123! (iconified) ";
static std::string g_display;

static std::string zoomSocketPath() {
    std::string d = g_display;
    for (char& ch : d) if (ch == '/') ch = '_';
    return "/tmp/vcompmgr_" + d + ".sock";
}
static double getZoom() {
    static double cached = 1.0;
    static int since = 0;
    if (++since < 15) return cached;
    since = 0;
    FILE* f = fopen((zoomSocketPath() + ".zoom").c_str(), "r");
    double v = 1.0;
    if (f) { if (fscanf(f, "%lf", &v) != 1) v = 1.0; fclose(f); }
    cached = v;
    return cached;
}

static void sendZoom(double delta) {
    std::string p = zoomSocketPath();
    int fd = socket(AF_UNIX, SOCK_DGRAM, 0);
    if (fd < 0) return;
    struct sockaddr_un a;
    memset(&a, 0, sizeof(a));
    a.sun_family = AF_UNIX;
    strncpy(a.sun_path, p.c_str(), sizeof(a.sun_path) - 1);
    char buf[32];
    snprintf(buf, sizeof(buf), "zoom %+.2f", delta);
    sendto(fd, buf, strlen(buf), 0, (struct sockaddr*)&a, sizeof(a));
    close(fd);
}

//helper for atoms intern
static xcb_atom_t internAtom(xcb_connection_t* conn, const char* name) {
    xcb_intern_atom_cookie_t c = xcb_intern_atom(conn, 0, strlen(name), name);
    xcb_intern_atom_reply_t* r = xcb_intern_atom_reply(conn, c, nullptr);
    xcb_atom_t a = r ? r->atom : 0;
    free(r);
    return a;
}
void KillApp(xcb_connection_t* conn, xcb_window_t win) {
    auto* prop = xcb_get_property_reply(
        conn, xcb_get_property(conn, 0, win, wm_protocols_atom,
                               XCB_ATOM_ATOM, 0, 1024), nullptr);
    bool supported = false;
    if (prop && prop->type == XCB_ATOM_ATOM && prop->format == 32) {
        xcb_atom_t* atoms = (xcb_atom_t*)xcb_get_property_value(prop);
        for (uint32_t i = 0; i < prop->value_len; ++i)
            if (atoms[i] == wm_delete_window_atom) { supported = true; break; }
    }
    free(prop);
    if (supported) {
        xcb_client_message_event_t m{};
        m.response_type = XCB_CLIENT_MESSAGE;
        m.format = 32;
        m.window = win;
        m.type = wm_protocols_atom;
        m.data.data32[0] = wm_delete_window_atom;
        m.data.data32[1] = XCB_CURRENT_TIME;
        xcb_send_event(conn, 0, win, XCB_EVENT_MASK_NO_EVENT, (const char*)&m);
    } else {
        xcb_kill_client(conn, win);
    }
    xcb_flush(conn);
}
void SetWallpapers() {

}
static xcb_cursor_t createFontCursor(xcb_connection_t* conn, uint16_t glyph) {
    xcb_font_t font = xcb_generate_id(conn);
    xcb_open_font(conn, font, strlen("cursor"), "cursor");
    xcb_cursor_t cur = xcb_generate_id(conn);
    xcb_create_glyph_cursor(conn, cur, font, font, glyph, glyph,
                            0, 0, 0, 0xffff, 0xffff, 0xffff);
    xcb_close_font(conn, font);
    return cur;
}

void ShowName(xcb_connection_t* conn, xcb_window_t win) {
 if (titlewindow) {
  if (clients.empty()) return;
 xcb_change_property (conn, XCB_PROP_MODE_REPLACE, win, wm_name_atom, XCB_ATOM_STRING, 8, strlen (title), title);
  xcb_change_property (conn, XCB_PROP_MODE_REPLACE, win, wm_icon_name_atom, XCB_ATOM_STRING, 8, strlen(title_icon), title_icon);
  xcb_flush(conn);
  }
}
 void spawn(const char *cmd) {
    pid_t pid = fork();
    if (pid == 0) {
        setsid();
        std::string ds = "DISPLAY=" + g_display;
        putenv((char*)ds.c_str());
        execlp("/bin/sh", "sh", "-c", cmd, (char *)NULL);
        exit(1);
    }
}
void SpawnAtStartup(xcb_connection_t* conn) {
  if (!Startup) return;
  std::vector<std::string> appsatstartup = {
    "kitty",
    "firefox"
  };
 for (const auto& app : appsatstartup) {
   spawn(app.c_str());
 }
}
void focusClient(xcb_connection_t* conn, size_t idx) {
    if (clients.empty()) return;
    focusedIndex = idx % clients.size();
    xcb_window_t win = clients[focusedIndex].win;

    uint32_t stackMode = XCB_STACK_MODE_ABOVE;
    xcb_configure_window(conn, win, XCB_CONFIG_WINDOW_STACK_MODE, &stackMode);

    xcb_set_input_focus(conn, XCB_INPUT_FOCUS_PARENT, win, XCB_CURRENT_TIME);
    xcb_flush(conn);
}

typedef struct {
  int viewX;
  int viewY;
  double accumX;
  double accumY;
} View;

void updateBaseSize(xcb_window_t win, int w, int h);

static void applyWindowGeometry(xcb_connection_t* conn, ClientRec& c, const View& view) {
    int x = c.isFullscreen ? 0 : c.baseX - view.viewX;
    int y = c.isFullscreen ? 0 : c.baseY - view.viewY;
    int w = c.isFullscreen ? screenW : c.baseW;
    int h = c.isFullscreen ? screenH : c.baseH;
    const uint32_t vals[] = { (uint32_t)x, (uint32_t)y, (uint32_t)w, (uint32_t)h };
    xcb_configure_window(conn, c.win,
                         XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y |
                         XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT, vals);
}

static void sendFullscreenMsg(xcb_connection_t* conn, xcb_window_t win, int action) {
    xcb_client_message_event_t m{};
    m.response_type = XCB_CLIENT_MESSAGE;
    m.format = 32;
    m.window = win;
    m.type = net_wm_state_atom;
    m.data.data32[0] = action;
    m.data.data32[1] = net_wm_state_fullscreen_atom;
    m.data.data32[2] = 0;
    m.data.data32[3] = 1;
    xcb_send_event(conn, 0, win,
                   XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT, (const char*)&m);
    xcb_flush(conn);
}

static void toggleFullscreen(xcb_connection_t* conn) {
    if (clients.empty()) return;
    ClientRec& c = clients[focusedIndex];
    if (!c.isFullscreen) {
        c.saveX = c.baseX; c.saveY = c.baseY;
        c.saveW = c.baseW; c.saveH = c.baseH;
    } else {
        c.baseX = c.saveX; c.baseY = c.saveY;
        c.baseW = c.saveW; c.baseH = c.saveH;
    }
    c.isFullscreen = !c.isFullscreen;
    applyWindowGeometry(conn, c, {0, 0, 0.0, 0.0});
    sendFullscreenMsg(conn, c.win, c.isFullscreen ? 1 : 0);
    uint32_t stackMode = XCB_STACK_MODE_ABOVE;
    xcb_configure_window(conn, c.win, XCB_CONFIG_WINDOW_STACK_MODE, &stackMode);
    xcb_set_input_focus(conn, XCB_INPUT_FOCUS_PARENT, c.win, XCB_CURRENT_TIME);
    xcb_flush(conn);
}

void ResizingWindow(xcb_connection_t* conn, xcb_window_t win, View& view) {
  auto* resizingval = ResizingValue;
  xcb_configure_window(conn, win,  XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT, resizingval);
  updateBaseSize(win, resizingval[0], resizingval[1]);
  xcb_flush(conn);
}

void pan(xcb_connection_t* conn, View& view, int dx, int dy) {
    double z = getZoom();
    view.accumX -= dx / z;
    view.accumY -= dy / z;
    int ddx = (int)view.accumX;
    int ddy = (int)view.accumY;
    view.accumX -= ddx;
    view.accumY -= ddy;
    view.viewX += ddx;
    view.viewY += ddy;
    if (!infinity) {
        int maxViewX = (int)CanvasSize - screenW;
        int maxViewY = (int)CanvasSize - screenH;
        if (maxViewX < 0) maxViewX = 0;
        if (maxViewY < 0) maxViewY = 0;
        view.viewX = std::clamp(view.viewX, 0, maxViewX);
        view.viewY = std::clamp(view.viewY, 0, maxViewY);
    }
    for (auto& c : clients) {
        if (!infinity) {
            c.baseX = std::clamp(c.baseX, 0, (int)CanvasSize - c.baseW);
            c.baseY = std::clamp(c.baseY, 0, (int)CanvasSize - c.baseH);
        }
        applyWindowGeometry(conn, c, view);
    }
    xcb_flush(conn);
}

void updateBaseSize(xcb_window_t win, int w, int h) {
    for (auto& c : clients)
        if (c.win == win) { c.baseW = w; c.baseH = h; }
}

int main(int argc, char* argv[]) {
    std::string display = argc > 1 ? argv[1] : "";
    g_display = display.empty() ? ":0" : display;

    pid_t vcompPid = fork();
    if (vcompPid == 0) {
        setsid();
        std::string ds = "DISPLAY=" + g_display;
        putenv((char*)ds.c_str());
        execlp("vcompmgr", "vcompmgr", (char*)nullptr);
        exit(1);
    }

    pid_t kbdPid = fork();
    if (kbdPid == 0) {
        setsid();
        std::string ds = "DISPLAY=" + g_display;
        putenv((char*)ds.c_str());
        execlp("/bin/sh", "sh", "-c", layoutCmd, (char*)nullptr);
        exit(1);
    }

    int screen_num = 0;
    xcb_connection_t* conn = xcb_connect(display.empty() ? nullptr : display.c_str(), &screen_num);

    if (xcb_connection_has_error(conn)) {
        std::cerr << "[ERROR - 1] spacewm: failed to connect to X server" << std::endl;
        return 1;
    }

    const xcb_setup_t* setup = xcb_get_setup(conn);
    xcb_screen_iterator_t it = xcb_setup_roots_iterator(setup);
    for (int i = 0; i < screen_num; ++i)
        xcb_screen_next(&it);
    xcb_screen_t* screen = it.data;

    std::cout << "[x] spacewm: connected (screen " << screen_num << ")\n"
              << "  root: 0x" << std::hex << screen->root << std::dec << '\n'
              << "  size: " << screen->width_in_pixels << "x" << screen->height_in_pixels << std::endl;
    rootWin = screen->root;

uint32_t rootbg = screen->white_pixel;
xcb_change_window_attributes(conn, screen->root, XCB_CW_BACK_PIXEL, &rootbg);
xcb_clear_area(conn, 1, screen->root, 0, 0, 0, 0);
xcb_flush(conn);

uint32_t evmask = XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT
                | XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY
                | XCB_EVENT_MASK_BUTTON_PRESS
                | XCB_EVENT_MASK_BUTTON_RELEASE
                | XCB_EVENT_MASK_POINTER_MOTION;
xcb_change_window_attributes(conn, screen->root, XCB_CW_EVENT_MASK, &evmask);
xcb_flush(conn);

xcb_cursor_t arrowCursor = createFontCursor(conn, XC_left_ptr);
xcb_cursor_t moveCursor  = createFontCursor(conn, XC_fleur);
xcb_cursor_t resizeCursor = createFontCursor(conn, XC_sizing);
uint32_t cvals[] = { arrowCursor };
xcb_change_window_attributes(conn, screen->root, XCB_CW_CURSOR, cvals);
xcb_flush(conn);

xcb_key_symbols_t* ksyms = xcb_key_symbols_alloc(conn);
wm_name_atom = internAtom(conn, "WM_NAME");
wm_icon_name_atom = internAtom(conn, "WM_ICON_NAME");
net_wm_state_atom = internAtom(conn, "_NET_WM_STATE");
net_wm_state_fullscreen_atom = internAtom(conn, "_NET_WM_STATE_FULLSCREEN");
wm_protocols_atom = internAtom(conn, "WM_PROTOCOLS");
wm_delete_window_atom = internAtom(conn, "WM_DELETE_WINDOW");

screenW = screen->width_in_pixels;
screenH = screen->height_in_pixels;
grabBindings(conn, screen->root, ksyms);

View view = { 0, 0, 0.0, 0.0 };
int mode = 0;  
int last_x = 0, last_y = 0;
xcb_window_t resizeWin = 0;
int resizeRX = 0, resizeRY = 0, resizeW = 0, resizeH = 0;
int spawnN = 0;

if (autostart) {
    wmstarter st;
    st.AutomateSetup();
    st.SetupXinitrc();
    st.SetupConsoleAutostart();
    SpawnAtStartup(conn);
}

xcb_generic_event_t* ev;
while ((ev = xcb_wait_for_event(conn))) {
    switch (ev->response_type & ~0x80) {
    case XCB_EXPOSE:
        break;
    case XCB_KEY_PRESS: {
        auto* kp = (xcb_key_press_event_t*)ev;
        uint32_t state = kp->state & ~(XCB_MOD_MASK_LOCK | XCB_MOD_MASK_2);
        xcb_keysym_t ksym = xcb_key_symbols_get_keysym(ksyms, kp->detail, 0);
        for (const Binding& b : bindings) {
            if (b.modmask != state || b.keysym != ksym) continue;
            switch (b.action) {
            case Action::FocusNext:
                focusClient(conn, focusedIndex + 1);
                break;
            case Action::FocusPrev:
                focusClient(conn, focusedIndex + clients.size() - 1);
                break;
            case Action::SpawnTerminal:
                spawn(terminal);
                break;
            case Action::Resize:
                if (!clients.empty())
                    ResizingWindow(conn, clients[focusedIndex].win, view);
                break;
            case Action::ToggleFullscreen:
                toggleFullscreen(conn);
                break;
            case Action::Close:
                if (!clients.empty())
                    KillApp(conn, clients[focusedIndex].win); //if u want new action change this :3 
                break;
            case Action::SpawnApp:
                spawn(terminal);
                break;
            }
            break;
        }
        break;
    }
    case XCB_KEY_RELEASE:
        break;
    case XCB_BUTTON_PRESS: {
        auto* bp = (xcb_button_press_event_t*)ev;
        if (bp->detail == 4 || bp->detail == 5) {
            sendZoom(bp->detail == 4 ? 0.15 : -0.15);
            xcb_allow_events(conn, XCB_ALLOW_ASYNC_POINTER, XCB_CURRENT_TIME);
            xcb_flush(conn);
            break;
        }
        if (bp->event == screen->root) {
            if (bp->detail == 1) {
                mode = 1;
                last_x = bp->root_x;
                last_y = bp->root_y;
                xcb_grab_pointer(conn, 0, screen->root,
                                 XCB_EVENT_MASK_BUTTON_RELEASE | XCB_EVENT_MASK_POINTER_MOTION,
                                 XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC,
                                 XCB_NONE, moveCursor, XCB_CURRENT_TIME);
                xcb_flush(conn);
            }
        } else {
            auto it = std::find_if(clients.begin(), clients.end(),
                                   [&](const ClientRec& c) { return c.win == bp->event; });
            if (it != clients.end()) {
                auto gq = xcb_get_geometry(conn, bp->event);
                auto* gr = xcb_get_geometry_reply(conn, gq, nullptr);
                bool corner = gr && gr->width >= 32 && gr->height >= 32
                              && (bp->event_x < ResizeEdge || bp->event_x > (int)gr->width - ResizeEdge)
                              && (bp->event_y < ResizeEdge || bp->event_y > (int)gr->height - ResizeEdge);
                bool side = gr && gr->width >= 32 && gr->height >= 32
                            && (bp->event_x < ResizeEdge || bp->event_x > (int)gr->width - ResizeEdge
                                || bp->event_y > (int)gr->height - ResizeEdge);
                if (corner || (side && bp->event_y > DragTopHeight)) {
                    mode = 2;
                    resizeWin = bp->event;
                    resizeRX = bp->root_x;
                    resizeRY = bp->root_y;
                    resizeW = gr->width;
                    resizeH = gr->height;
                    xcb_allow_events(conn, XCB_ALLOW_SYNC_POINTER, XCB_CURRENT_TIME);
                    xcb_flush(conn);
                } else if (bp->event_y <= DragTopHeight) {
                    focusClient(conn, (size_t)(it - clients.begin()));
                    mode = 3;
                    moveWin = bp->event;
                    moveRX = bp->root_x;
                    moveRY = bp->root_y;
                    moveBX = it->baseX;
                    moveBY = it->baseY;
                    pressX = bp->root_x;
                    pressY = bp->root_y;
                    clickPending = true;
                    xcb_allow_events(conn, XCB_ALLOW_SYNC_POINTER, XCB_CURRENT_TIME);
                    xcb_flush(conn);
                } else {
                    focusClient(conn, (size_t)(it - clients.begin()));
                    xcb_allow_events(conn, XCB_ALLOW_REPLAY_POINTER, XCB_CURRENT_TIME);
                    xcb_flush(conn);
                }
                free(gr);
            }
        }
        break;
    }
    case XCB_CONFIGURE_REQUEST: {
        auto* cr = (xcb_configure_request_event_t*)ev;
        bool geometryChanged = false;
        for (auto& c : clients) {
            if (c.win != cr->window) continue;
            if (cr->value_mask & XCB_CONFIG_WINDOW_WIDTH)  { c.baseW = cr->width;  geometryChanged = true; }
            if (cr->value_mask & XCB_CONFIG_WINDOW_HEIGHT) { c.baseH = cr->height; geometryChanged = true; }
            if (geometryChanged) applyWindowGeometry(conn, c, view);
            break;
        }
        uint32_t values[7];
        int i = 0;
        uint16_t mask = 0;
        if (cr->value_mask & XCB_CONFIG_WINDOW_X)            { values[i++] = cr->x;            mask |= XCB_CONFIG_WINDOW_X; }
        if (cr->value_mask & XCB_CONFIG_WINDOW_Y)            { values[i++] = cr->y;            mask |= XCB_CONFIG_WINDOW_Y; }
        if (cr->value_mask & XCB_CONFIG_WINDOW_WIDTH)        { values[i++] = cr->width;        mask |= XCB_CONFIG_WINDOW_WIDTH; }
        if (cr->value_mask & XCB_CONFIG_WINDOW_HEIGHT)       { values[i++] = cr->height;       mask |= XCB_CONFIG_WINDOW_HEIGHT; }
        if (cr->value_mask & XCB_CONFIG_WINDOW_BORDER_WIDTH) { values[i++] = cr->border_width; mask |= XCB_CONFIG_WINDOW_BORDER_WIDTH; }
        if (cr->value_mask & XCB_CONFIG_WINDOW_STACK_MODE)   { values[i++] = cr->stack_mode;   mask |= XCB_CONFIG_WINDOW_STACK_MODE; }
        xcb_configure_window(conn, cr->window, mask, values);
        xcb_flush(conn);
        break;
    }
    case XCB_MAP_REQUEST: {
        auto* mr = (xcb_map_request_event_t*)ev;
        std::cout << "[x] Map request for 0x" << std::hex << mr->window << std::dec << std::endl;
        auto gq = xcb_get_geometry(conn, mr->window);
        auto* gr = xcb_get_geometry_reply(conn, gq, nullptr);
        int w = gr ? gr->width : 400;
        int h = gr ? gr->height : 300;
        free(gr);
        int off = (spawnN % 8) * 40;
        int nx = view.viewX + (int)screen->width_in_pixels / 2 - w / 2 + off;
        int ny = view.viewY + (int)screen->height_in_pixels / 2 - h / 2 + off;
        if (!infinity) {
            nx = std::clamp(nx, 0, (int)CanvasSize - w);
            ny = std::clamp(ny, 0, (int)CanvasSize - h);
        }
        spawnN++;
        xcb_map_window(conn, mr->window);
        clients.push_back({ mr->window, nx, ny, w, h, 0, 0, 0, 0 });
        applyWindowGeometry(conn, clients.back(), view);
        xcb_grab_button(conn, 0, mr->window,
                        XCB_EVENT_MASK_BUTTON_PRESS | XCB_EVENT_MASK_BUTTON_RELEASE | XCB_EVENT_MASK_POINTER_MOTION,
                        XCB_GRAB_MODE_SYNC, XCB_GRAB_MODE_ASYNC,
                        XCB_NONE, resizeCursor, XCB_BUTTON_INDEX_ANY, XCB_MOD_MASK_ANY);
        xcb_flush(conn);
        break;
    }
    case XCB_DESTROY_NOTIFY: {
        auto* dn = (xcb_destroy_notify_event_t*)ev;
        auto it = std::find_if(clients.begin(), clients.end(),
                               [&](const ClientRec& c) { return c.win == dn->window; });
        if (it != clients.end()) {
            size_t idx = (size_t)(it - clients.begin());
            clients.erase(it);
            if (idx < focusedIndex) focusedIndex--;
            else if (idx == focusedIndex)
                focusedIndex = clients.empty() ? 0 : focusedIndex % clients.size();
        }
        break;
    }

    case XCB_MOTION_NOTIFY: {
        auto* mn = (xcb_motion_notify_event_t*)ev;
        if (mode == 1) {
            pan(conn, view, mn->root_x - last_x, mn->root_y - last_y);
            last_x = mn->root_x;
            last_y = mn->root_y;
        } else if (mode == 2) {
            int nw = resizeW + (mn->root_x - resizeRX);
            int nh = resizeH + (mn->root_y - resizeRY);
            if (nw < 20) nw = 20;
            if (nh < 20) nh = 20;
            if (!infinity) {
                if (nw > (int)CanvasSize) nw = (int)CanvasSize;
                if (nh > (int)CanvasSize) nh = (int)CanvasSize;
            }
            updateBaseSize(resizeWin, nw, nh);
            const uint32_t vals[] = { (uint32_t)nw, (uint32_t)nh };
            xcb_configure_window(conn, resizeWin,
                                 XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT, vals);
            xcb_flush(conn);
        } else if (mode == 3) {
            if (clickPending && (abs(mn->root_x - pressX) > 2 || abs(mn->root_y - pressY) > 2))
                clickPending = false;
            if (!clickPending) {
                auto it = std::find_if(clients.begin(), clients.end(),
                                       [&](const ClientRec& c) { return c.win == moveWin; });
                if (it != clients.end()) {
                    it->baseX = moveBX + (mn->root_x - moveRX);
                    it->baseY = moveBY + (mn->root_y - moveRY);
                    if (!infinity) {
                        it->baseX = std::clamp(it->baseX, 0, (int)CanvasSize - it->baseW);
                        it->baseY = std::clamp(it->baseY, 0, (int)CanvasSize - it->baseH);
                    }
                    applyWindowGeometry(conn, *it, view);
                }
                xcb_flush(conn);
            }
        }
        break;
    }

    case XCB_BUTTON_RELEASE: {
        auto* br = (xcb_button_release_event_t*)ev;
        if (mode == 3 && clickPending) {
            xcb_button_press_event_t p{};
            p.response_type = XCB_BUTTON_PRESS;
            p.detail = 1;
            p.time = br->time;
            p.root = rootWin;
            p.event = moveWin;
            p.state = 0;
            p.same_screen = 1;
            p.child = XCB_NONE;
            p.root_x = pressX;
            p.root_y = pressY;
            p.event_x = pressX - (clients.empty() ? 0 : clients[focusedIndex].baseX) + view.viewX;
            p.event_y = pressY - (clients.empty() ? 0 : clients[focusedIndex].baseY) + view.viewY;
            xcb_send_event(conn, 0, moveWin, XCB_EVENT_MASK_BUTTON_PRESS, (const char*)&p);
            xcb_button_release_event_t r{};
            r.response_type = XCB_BUTTON_RELEASE;
            r.detail = 1;
            r.time = br->time;
            r.root = rootWin;
            r.event = moveWin;
            r.state = 0;
            r.same_screen = 1;
            r.child = XCB_NONE;
            r.root_x = pressX;
            r.root_y = pressY;
            r.event_x = p.event_x;
            r.event_y = p.event_y;
            xcb_send_event(conn, 0, moveWin, XCB_EVENT_MASK_BUTTON_RELEASE, (const char*)&r);
        }
        mode = 0;
        clickPending = false;
        xcb_ungrab_pointer(conn, XCB_CURRENT_TIME);
        xcb_flush(conn);
        break;
    }
    }
    free(ev);
}

    xcb_disconnect(conn);
    return 0;
}
