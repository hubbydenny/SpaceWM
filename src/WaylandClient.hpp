#include <wayland-client.h>
#include <wayland-client-protocol.h>
#include "xdg-shell-client-protocol.h"
#include <iostream>
#include <cstring>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "config.hpp"
#include <xkbcommon/xkbcommon.h>

class WaylandClient {
public:
    WaylandClient();
    ~WaylandClient();
    bool init();
    void create_surface();
    void run();
    void setConfig(Config* cfg);
private:
    wl_display*    m_display    = nullptr;
    wl_registry*   m_registry   = nullptr;
    wl_compositor* m_compositor = nullptr;
    wl_shm*        m_shm        = nullptr;
    wl_seat*       m_seat       = nullptr;
    wl_keyboard*   m_keyboard   = nullptr;
    wl_surface*    m_surface    = nullptr;
    wl_buffer*     m_buffer     = nullptr;
    int*           m_pixels     = nullptr;
    int            m_width      = 400;
    int            m_height     = 300;
    Config*        m_cfg        = nullptr;
    xkb_context*   m_xkb_context = nullptr;
    xkb_keymap*    m_keymap     = nullptr;
    xkb_state*     m_state      = nullptr;
    xdg_wm_base*   m_xdg_wm_base  = nullptr;
    xdg_surface*   m_xdg_surface  = nullptr;
    xdg_toplevel*  m_xdg_toplevel = nullptr;
    bool           m_configured  = false;
    uint32_t       m_mods        = 0;

    static void registry_global(void*, wl_registry*, uint32_t, const char*, uint32_t);
    static void registry_global_remove(void*, wl_registry*, uint32_t);
    static constexpr wl_registry_listener s_registry_listener = {registry_global, registry_global_remove};

    static void seat_capabilities(void*, wl_seat*, uint32_t);
    static void seat_name(void*, wl_seat*, const char*);
    
    static constexpr wl_seat_listener s_seat_listener = {seat_capabilities, seat_name};

    static void keyboard_keymap(void*, wl_keyboard*, uint32_t, int32_t, uint32_t);
    static void keyboard_enter(void*, wl_keyboard*, uint32_t, wl_surface*, wl_array*);
    static void keyboard_leave(void*, wl_keyboard*, uint32_t, wl_surface*);
    static void keyboard_key(void*, wl_keyboard*, uint32_t, uint32_t, uint32_t, uint32_t);
    static void keyboard_modifiers(void*, wl_keyboard*, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);
    static void keyboard_repeat_info(void*, wl_keyboard*, int32_t, int32_t);
    static constexpr wl_keyboard_listener s_keyboard_listener = {keyboard_keymap, keyboard_enter, keyboard_leave, keyboard_key, keyboard_modifiers, keyboard_repeat_info};

    static void xdg_wm_base_ping(void*, xdg_wm_base*, uint32_t);
    static constexpr xdg_wm_base_listener s_xdg_wm_base_listener = {xdg_wm_base_ping};

    static void xdg_surface_configure(void*, xdg_surface*, uint32_t);
    static constexpr xdg_surface_listener s_xdg_surface_listener = {xdg_surface_configure};

    static void xdg_toplevel_configure(void*, xdg_toplevel*, int32_t, int32_t, wl_array*);
    static void xdg_toplevel_close(void*, xdg_toplevel*);
    static void xdg_toplevel_configure_bounds(void*, xdg_toplevel*, int32_t, int32_t);
    static void xdg_toplevel_wm_capabilities(void*, xdg_toplevel*, wl_array*);
    static constexpr xdg_toplevel_listener s_xdg_toplevel_listener = {xdg_toplevel_configure, xdg_toplevel_close, xdg_toplevel_configure_bounds, xdg_toplevel_wm_capabilities};

    struct UniqueFd {
        int fd;
        explicit UniqueFd(int f) : fd(f) {}
        ~UniqueFd() { if (fd > 0) ::close(fd); }
        UniqueFd(const UniqueFd&) = delete;
        UniqueFd& operator=(const UniqueFd&) = delete;
        int get() const { return fd; }
    };
    static UniqueFd create_shm_fd(std::size_t);
    void create_shm_buffer();
};
