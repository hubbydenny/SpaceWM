#pragma once

#include <wayland-server.h>
#include <wayland-server-core.h>
#include <wayland-server-protocol.h>
#include <wayland-util.h>
#include <string>
#include <map>
#include "animations.hpp"
#include "wconf.hpp"

struct wlr_backend;
struct wlr_renderer;
struct wlr_allocator;
struct wlr_compositor;
struct wlr_output;
struct wlr_scene_tree;
struct wlr_scene_rect;
struct wlr_xdg_toplevel;
struct wlr_xdg_surface;
struct wlr_output_layout;
struct wlr_output_layout_output;
struct wlr_scene;
struct wlr_scene_output;
struct wlr_scene_output_layout;
struct wlr_scene_tree;
struct wlr_scene_rect;
struct wlr_xdg_shell;
struct wlr_xdg_toplevel;
struct wlr_xdg_surface;
struct wlr_seat;
struct wlr_keyboard;
struct wlr_pointer;
struct wlr_cursor;
struct wlr_xcursor_manager;
struct wlr_input_device;
struct xkb_context;
struct xkb_keymap;
struct xkb_state;

class Compositor;

struct View {
    struct MoveState {
        int  click_x = 0, click_y = 0;
        int  win_x   = 0, win_y   = 0;
    };
    struct ResizeState {
        int  click_x = 0, click_y = 0;
        int  orig_x = 0, orig_y = 0;
        int  win_w   = 0, win_h   = 0;
        uint32_t edges = 0;
    };
    enum class Grab { None, Move, Resize };
    Grab               grab = Grab::None;
    MoveState          move;
    ResizeState        resize;

    Compositor*        comp        = nullptr;
    wlr_xdg_toplevel* toplevel    = nullptr;
    wlr_xdg_surface*  surface     = nullptr;
    wlr_scene_tree*   tree        = nullptr;
    wlr_scene_tree*   border_tree = nullptr;
    wlr_scene_rect*   border_top    = nullptr;
    wlr_scene_rect*   border_bottom = nullptr;
    wlr_scene_rect*   border_left   = nullptr;
    wlr_scene_rect*   border_right  = nullptr;
    wl_listener        commit{};
    wl_listener        destroy{};
    wl_listener        request_move{};
    wl_listener        request_resize{};
    wl_listener        request_fullscreen{};
    bool               is_focused  = false;
    bool               fullscreen  = false;
    bool               floating    = false;
    int                saved_x = 0, saved_y = 0;
};

struct Cursor {
    Compositor*         comp    = nullptr;
    wlr_cursor*         cursor  = nullptr;
    wlr_xcursor_manager* xcursor = nullptr;
    wl_listener         motion{};
    wl_listener         motion_absolute{};
    wl_listener         button{};
    wl_listener         axis{};
    wl_listener         frame{};
    View*               view_under = nullptr;
};

struct Keyboard {
    Compositor*        comp;
    wlr_keyboard*     wlr_kb;
    xkb_context*       ctx;
    xkb_keymap*        keymap;
    xkb_state*         state;
    wl_listener        modifiers{};
    wl_listener        key{};
};

struct OutputCtx {
    Compositor*        comp;
    wlr_output*       output;
    wlr_scene_output* scene_out;
    wl_listener       frame{};
    wl_listener       destroy{};
};

class Compositor {
public:
    Compositor() = default;
    ~Compositor();
    bool init();
    void run();
    std::string socket() const { return socket_; }

    View*          viewCreate(wlr_xdg_toplevel*, wlr_scene_tree* parent);
    void           viewDestroy(View*);
    void           viewFocus(View*);
    View*          viewAt(int x, int y);
    void           cursorInit();
    void           cursorReconf();
    void           toggleFullscreen(View*);

private:
    wl_display*                display_     = nullptr;
    wlr_backend*               backend_     = nullptr;
    wlr_renderer*             renderer_    = nullptr;
    wlr_allocator*            allocator_   = nullptr;
    wlr_compositor*           compositor_ = nullptr;
    wlr_xdg_shell*            xdg_shell_  = nullptr;
    wlr_seat*                 seat_        = nullptr;
    wlr_output_layout*        output_layout_ = nullptr;
    wlr_scene*                scene_      = nullptr;
    wlr_scene_output_layout*  sol_        = nullptr;
    wlr_scene_rect*          background_ = nullptr;
    int                       output_w_   = 1280;
    int                       output_h_   = 720;
    Animations                anims_;
    std::string               socket_;

    Keyboard*                 kbd_        = nullptr;
    Cursor*                   cursor_     = nullptr;
    View*                     focused_view_ = nullptr;
    std::map<wlr_xdg_toplevel*, View*> views_;

    wl_listener new_output_{};
    wl_listener new_input_{};
    wl_listener new_toplevel_{};

    void onOutput(wlr_output* out);
    void onInput(wlr_input_device* dev);
    void onToplevel(wlr_xdg_toplevel* t);
    void setFocusedView(View*);
    static void cbOutput(wl_listener*, void*);
    static void cbInput(wl_listener*, void*);
    static void cbToplevel(wl_listener*, void*);

    friend void run_action(Compositor*, wconf::Action, const std::string&);
    friend void cbKeyboardKey(wl_listener*, void*);
    friend void cbKeyboardMod(wl_listener*, void*);
};
