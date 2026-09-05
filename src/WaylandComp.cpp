#include "WaylandComp.hpp"
#include "animations.hpp"
#include "wconf.hpp"
#include "wlr_compat.hpp"

void run_action(Compositor* comp, wconf::Action a, const std::string& arg);
void cbKeyboardKey(wl_listener* l, void* data);
void cbKeyboardMod(wl_listener* l, void* data);

#include <iostream>
#include <string>
#include <cstddef>
#include <cstring>
#include <ctime>
#include <algorithm>
#include <unistd.h>
#include <sys/wait.h>

#include <wlr/backend.h>
#include <wlr/render/allocator.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/render/gles2.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_subcompositor.h>
#include <wlr/types/wlr_data_device.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/types/wlr_seat.h>
#include <wlr/types/wlr_keyboard.h>
#include <wlr/types/wlr_pointer.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_xcursor_manager.h>
#include <wlr/util/log.h>
#include <xkbcommon/xkbcommon.h>
#include <xkbcommon/xkbcommon-compose.h>
#include <linux/input-event-codes.h>
#include <GLES2/gl2.h>

Compositor::~Compositor() {
    if (kbd_) {
        if (kbd_->state) xkb_state_unref(kbd_->state);
        if (kbd_->keymap) xkb_keymap_unref(kbd_->keymap);
        if (kbd_->ctx) xkb_context_unref(kbd_->ctx);
        delete kbd_;
    }
    if (cursor_) {
        if (cursor_->xcursor) wlr_xcursor_manager_destroy(cursor_->xcursor);
        if (cursor_->cursor) wlr_cursor_destroy(cursor_->cursor);
        delete cursor_;
    }
    for (auto& [k, v] : views_) delete v;
    if (display_) wl_display_destroy(display_);
}

bool Compositor::init() {
    wlr_log_init(WLR_DEBUG, nullptr);

    display_ = wl_display_create();
    if (!display_) { std::cerr << "[comp] wl_display_create failed\n"; return false; }

    auto* loop = wl_display_get_event_loop(display_);
    backend_ = wlr_backend_autocreate(loop, nullptr);
    if (!backend_) { std::cerr << "[comp] wlr_backend_autocreate failed\n"; return false; }

    renderer_ = wlr_renderer_autocreate(backend_);
    if (!renderer_) { std::cerr << "[comp] wlr_renderer_autocreate failed\n"; return false; }
    wlr_renderer_init_wl_display(renderer_, display_);

    allocator_ = wlr_allocator_autocreate(backend_, renderer_);
    if (!allocator_) { std::cerr << "[comp] wlr_allocator_autocreate failed\n"; return false; }

    output_layout_ = wlr_output_layout_create(display_);
    if (!output_layout_) { std::cerr << "[comp] wlr_output_layout_create failed\n"; return false; }

    scene_ = wlr_scene_create();
    if (!scene_) { std::cerr << "[comp] wlr_scene_create failed\n"; return false; }

    background_ = wlr_scene_rect_create(&scene_->tree, 0, 0, (float[4]){0.15f, 0.15f, 0.15f, 1.0f});

    sol_ = wlr_scene_attach_output_layout(scene_, output_layout_);
    if (!sol_) { std::cerr << "[comp] wlr_scene_attach_output_layout failed\n"; return false; }

    compositor_ = wlr_compositor_create(display_, 6, renderer_);
    if (!compositor_) { std::cerr << "[comp] wlr_compositor_create failed\n"; return false; }

    wlr_subcompositor_create(display_);
    wlr_data_device_manager_create(display_);

    xdg_shell_ = wlr_xdg_shell_create(display_, 3);
    if (!xdg_shell_) { std::cerr << "[comp] wlr_xdg_shell_create failed\n"; return false; }

    seat_ = wlr_seat_create(display_, "seat0");
    if (!seat_) { std::cerr << "[comp] wlr_seat_create failed\n"; return false; }
    wlr_seat_set_capabilities(seat_,
        WL_SEAT_CAPABILITY_KEYBOARD | WL_SEAT_CAPABILITY_POINTER);

    cursorInit();

    new_output_.notify  = cbOutput;
    new_input_.notify   = cbInput;
    new_toplevel_.notify = [](wl_listener* l, void* d) {
        auto* c = reinterpret_cast<Compositor*>(
            reinterpret_cast<char*>(l) - offsetof(Compositor, new_toplevel_));
        c->onToplevel(static_cast<wlr_xdg_toplevel*>(d));
    };

    wl_signal_add(&backend_->events.new_output,    &new_output_);
    wl_signal_add(&backend_->events.new_input,     &new_input_);
    wl_signal_add(&xdg_shell_->events.new_toplevel, &new_toplevel_);

    auto* sock = "spacewc-0";
    if (wl_display_add_socket(display_, sock)) {
        std::cerr << "[comp] wl_display_add_socket failed\n";
        return false;
    }
    socket_ = sock;
    setenv("WAYLAND_DISPLAY", sock, 1);
    std::cout << "[comp] WAYLAND_DISPLAY=" << socket_ << "\n";

    if (!wlr_backend_start(backend_)) {
        std::cerr << "[comp] wlr_backend_start failed\n";
        return false;
    }
    return true;
}

void Compositor::run() {
    std::cout << "[comp] running\n";
    auto* loop = wl_display_get_event_loop(display_);
    auto* timer = wl_event_loop_add_timer(loop,
        [](void* data) -> int {
            auto* comp = static_cast<Compositor*>(data);
            struct timespec ts;
            clock_gettime(CLOCK_MONOTONIC, &ts);
            uint32_t now = (uint32_t)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
            comp->anims_.tick(now);
            return 0;
        }, this);
    wl_event_source_timer_update(timer, 16);
    wl_display_run(display_);
}

View* Compositor::viewCreate(wlr_xdg_toplevel* t, wlr_scene_tree* parent) {
    auto* view = new View;
    view->comp      = this;
    view->toplevel  = t;
    view->surface   = t->base;
    view->tree      = wlr_scene_xdg_surface_create(parent, t->base);
    t->base->data   = view;

    if (wconf::get().border.enabled) {
        view->border_tree = wlr_scene_tree_create(parent);
        view->border_tree->node.data = view;
        int B = wconf::get().border.width;
        float col[4] = {0.2f, 0.2f, 0.2f, 1.0f};
        view->border_top    = wlr_scene_rect_create(view->border_tree, 1, B, col);
        view->border_bottom = wlr_scene_rect_create(view->border_tree, 1, B, col);
        view->border_left   = wlr_scene_rect_create(view->border_tree, B, 1, col);
        view->border_right  = wlr_scene_rect_create(view->border_tree, B, 1, col);
        wlr_scene_node_set_position(&view->border_tree->node, 0, 0);
        wlr_scene_node_set_position(&view->border_top->node,    0, 0);
        wlr_scene_node_set_position(&view->border_bottom->node, 0, 0);
        wlr_scene_node_set_position(&view->border_left->node,   0, 0);
        wlr_scene_node_set_position(&view->border_right->node,  0, 0);
    }

    return view;
}

void Compositor::viewDestroy(View* view) {
    if (!view) return;
    if (focused_view_ == view) focused_view_ = nullptr;
    views_.erase(view->toplevel);
    wl_list_remove(&view->commit.link);
    wl_list_remove(&view->destroy.link);
    wl_list_remove(&view->request_move.link);
    wl_list_remove(&view->request_resize.link);
    wl_list_remove(&view->request_fullscreen.link);
    if (view->border_tree) wlr_scene_node_destroy(&view->border_tree->node);
    delete view;
}

static void setBorderColor(View* view, bool focused) {
    if (!view) return;
    auto& b = wconf::get().border;
    auto& c = focused ? b.color_focused : b.color_unfocused;
    float col[4] = {c[0], c[1], c[2], c[3]};
    if (view->border_top)     wlr_scene_rect_set_color(view->border_top,     col);
    if (view->border_bottom)  wlr_scene_rect_set_color(view->border_bottom,  col);
    if (view->border_left)   wlr_scene_rect_set_color(view->border_left,   col);
    if (view->border_right)  wlr_scene_rect_set_color(view->border_right,  col);
}

static void updateViewBorders(View* view) {
    if (!view || !view->border_tree || !view->toplevel) return;
    auto* surf = view->toplevel->base;
    if (!surf || !surf->surface) return;

    int W = surf->surface->current.width;
    int H = surf->surface->current.height;
    if (W <= 0 || H <= 0) return;

    int B = wconf::get().border.width;

    wlr_scene_rect_set_size(view->border_top,    W + B * 2, B);
    wlr_scene_rect_set_size(view->border_bottom, W + B * 2, B);
    wlr_scene_rect_set_size(view->border_left,   B, H);
    wlr_scene_rect_set_size(view->border_right,  B, H);

    int vx = view->tree->node.x;
    int vy = view->tree->node.y;

    wlr_scene_node_set_position(&view->border_tree->node, vx - B, vy - B);
    wlr_scene_node_set_position(&view->border_top->node,    0,      0);
    wlr_scene_node_set_position(&view->border_bottom->node, 0,      H + B);
    wlr_scene_node_set_position(&view->border_left->node,   0,      B);
    wlr_scene_node_set_position(&view->border_right->node,  W + B,  B);
}
void Compositor::setFocusedView(View* view) {
    if (focused_view_ == view) return;
    if (focused_view_) {
        focused_view_->is_focused = false;
        setBorderColor(focused_view_, false);
    }
    focused_view_ = view;
    if (view) {
        view->is_focused = true;
        setBorderColor(view, true);
        anim_scale(anims_, &view->tree->node, 1.0f, 100);
    }
}

View* Compositor::viewAt(int x, int y) {
    double nx, ny;
    wlr_scene_node* node = wlr_scene_node_at(&scene_->tree.node, x, y, &nx, &ny);
    while (node) {
        if (node->data) {
            auto* v = static_cast<View*>(node->data);
            if (v->toplevel) return v;
        }
        node = node->parent ? &node->parent->node : nullptr;
    }
    return nullptr;
}

void Compositor::processCursorMotion(uint32_t time_msec) {
    if (!cursor_ || !cursor_->cursor) return;

    auto* view = viewAt((int)cursor_->cursor->x, (int)cursor_->cursor->y);
    if (view && view->grab == View::Grab::Move) {
        int dx = (int)cursor_->cursor->x - view->move.click_x;
        int dy = (int)cursor_->cursor->y - view->move.click_y;
        wlr_scene_node_set_position(&view->tree->node,
            view->move.win_x + dx, view->move.win_y + dy);
        updateViewBorders(view);
        return;
    }
    if (view && view->grab == View::Grab::Resize) {
        int dx = (int)cursor_->cursor->x - view->resize.click_x;
        int dy = (int)cursor_->cursor->y - view->resize.click_y;
        int new_w = view->resize.win_w;
        int new_h = view->resize.win_h;
        if (view->resize.edges & WLR_EDGE_RIGHT)  new_w += dx;
        if (view->resize.edges & WLR_EDGE_LEFT)   new_w -= dx;
        if (view->resize.edges & WLR_EDGE_BOTTOM) new_h += dy;
        if (view->resize.edges & WLR_EDGE_TOP)    { new_h -= dy; }
        new_w = std::max(50, new_w);
        new_h = std::max(50, new_h);
        wlr_xdg_toplevel_set_size(view->toplevel, new_w, new_h);
        return;
    }

    double sx = 0, sy = 0;
    wlr_scene_node* node = wlr_scene_node_at(&scene_->tree.node,
        cursor_->cursor->x, cursor_->cursor->y, &sx, &sy);

    wlr_surface* surface = nullptr;
    if (node && node->type == WLR_SCENE_NODE_BUFFER) {
        auto* scene_buffer = wlr_scene_buffer_from_node(node);
        auto* scene_surface = wlr_scene_surface_try_from_buffer(scene_buffer);
        if (scene_surface) {
            surface = scene_surface->surface;
        }
    }

    if (surface) {
        wlr_seat_pointer_notify_enter(seat_, surface, sx, sy);
        wlr_seat_pointer_notify_motion(seat_, time_msec, sx, sy);
    } else {
        if (cursor_->xcursor) {
            wlr_cursor_set_xcursor(cursor_->cursor, cursor_->xcursor, "left_ptr");
        }
        wlr_seat_pointer_clear_focus(seat_);
    }
}

void Compositor::cursorInit() {
    cursor_ = new Cursor;
    cursor_->comp = this;
    cursor_->cursor = wlr_cursor_create();
    if (!cursor_->cursor) { std::cerr << "[comp] wlr_cursor_create failed\n"; delete cursor_; cursor_ = nullptr; return; }
    wlr_cursor_attach_output_layout(cursor_->cursor, output_layout_);
    cursor_->xcursor = wlr_xcursor_manager_create(nullptr, 24);
    if (cursor_->xcursor) {
        wlr_xcursor_manager_load(cursor_->xcursor, 1.0f);
        wlr_cursor_set_xcursor(cursor_->cursor, cursor_->xcursor, "left_ptr");
    }

    cursor_->motion.notify = [](wl_listener* l, void* data) {
        auto* cur = reinterpret_cast<Cursor*>(
            reinterpret_cast<char*>(l) - offsetof(Cursor, motion));
        auto* ev = static_cast<wlr_pointer_motion_event*>(data);
        wlr_cursor_move(cur->cursor, ev->pointer ? &ev->pointer->base : nullptr,
            ev->delta_x, ev->delta_y);
        cur->comp->processCursorMotion(ev->time_msec);
    };
    wl_signal_add(&cursor_->cursor->events.motion, &cursor_->motion);

    cursor_->motion_absolute.notify = [](wl_listener* l, void* data) {
        auto* cur = reinterpret_cast<Cursor*>(
            reinterpret_cast<char*>(l) - offsetof(Cursor, motion_absolute));
        auto* ev = static_cast<wlr_pointer_motion_absolute_event*>(data);
        wlr_cursor_warp_absolute(cur->cursor, &ev->pointer->base, ev->x, ev->y);
        cur->comp->processCursorMotion(ev->time_msec);
    };
    wl_signal_add(&cursor_->cursor->events.motion_absolute, &cursor_->motion_absolute);

    cursor_->button.notify = [](wl_listener* l, void* data) {
        auto* cur = reinterpret_cast<Cursor*>(
            reinterpret_cast<char*>(l) - offsetof(Cursor, button));
        auto* ev = static_cast<wlr_pointer_button_event*>(data);
        wlr_seat_pointer_notify_button(cur->comp->seat_, ev->time_msec,
            ev->button, ev->state);
        if (ev->state == WL_POINTER_BUTTON_STATE_PRESSED) {
            auto* view = cur->comp->viewAt((int)cur->cursor->x, (int)cur->cursor->y);
            if (view) {
                cur->comp->setFocusedView(view);
                int mx = (int)cur->cursor->x;
                int my = (int)cur->cursor->y;
                int vx = view->tree->node.x;
                int vy = view->tree->node.y;
                int W = view->surface->surface->current.width;
                int H = view->surface->surface->current.height;
                int B = wconf::get().border.width;
                int TITLE_H = 30;

                bool near_left   = (mx >= vx - B && mx <= vx + 8);
                bool near_right  = (mx >= vx + W - 8 && mx <= vx + W + B);
                bool near_top    = (my >= vy - B && my <= vy + 8);
                bool near_bottom = (my >= vy + H - 8 && my <= vy + H + B);

                if (near_left || near_right || near_top || near_bottom) {
                    uint32_t edges = 0;
                    if (near_left)   edges |= WLR_EDGE_LEFT;
                    if (near_right)  edges |= WLR_EDGE_RIGHT;
                    if (near_top)    edges |= WLR_EDGE_TOP;
                    if (near_bottom) edges |= WLR_EDGE_BOTTOM;
                    view->grab = View::Grab::Resize;
                    view->resize.click_x = mx;
                    view->resize.click_y = my;
                    view->resize.orig_x = vx;
                    view->resize.orig_y = vy;
                    view->resize.win_w = W;
                    view->resize.win_h = H;
                    view->resize.edges = edges;
                } else if (my >= vy && my <= vy + TITLE_H) {
                    view->grab = View::Grab::Move;
                    view->move.click_x = mx;
                    view->move.click_y = my;
                    view->move.win_x = vx;
                    view->move.win_y = vy;
                }
            }
        }
        if (ev->state == WL_POINTER_BUTTON_STATE_RELEASED) {
            auto* view = cur->comp->viewAt((int)cur->cursor->x, (int)cur->cursor->y);
            if (view && view->grab != View::Grab::None) {
                view->grab = View::Grab::None;
            }
        }
    };
    wl_signal_add(&cursor_->cursor->events.button, &cursor_->button);

    cursor_->axis.notify = [](wl_listener* l, void* data) {
        auto* cur = reinterpret_cast<Cursor*>(
            reinterpret_cast<char*>(l) - offsetof(Cursor, axis));
        auto* ev = static_cast<wlr_pointer_axis_event*>(data);
        wlr_seat_pointer_notify_axis(cur->comp->seat_,
            ev->time_msec, ev->orientation, ev->delta,
            ev->delta_discrete, ev->source, ev->relative_direction);
    };
    wl_signal_add(&cursor_->cursor->events.axis, &cursor_->axis);

    cursor_->frame.notify = [](wl_listener* l, void*) {
        auto* cur = reinterpret_cast<Cursor*>(
            reinterpret_cast<char*>(l) - offsetof(Cursor, frame));
        wlr_seat_pointer_notify_frame(cur->comp->seat_);
    };
    wl_signal_add(&cursor_->cursor->events.frame, &cursor_->frame);

    cursor_->request_set_cursor.notify = [](wl_listener* l, void* data) {
        auto* cur = reinterpret_cast<Cursor*>(
            reinterpret_cast<char*>(l) - offsetof(Cursor, request_set_cursor));
        auto* ev = static_cast<wlr_seat_pointer_request_set_cursor_event*>(data);
        auto* focused_client = cur->comp->seat_->pointer_state.focused_client;
        if (focused_client == ev->seat_client) {
            wlr_cursor_set_surface(cur->cursor, ev->surface, ev->hotspot_x, ev->hotspot_y);
        }
    };
    wl_signal_add(&seat_->events.request_set_cursor, &cursor_->request_set_cursor);

    std::cout << "[comp] cursor ready\n";
}

void Compositor::cursorReconf() {
}

void Compositor::toggleFullscreen(View* view) {
    if (!view) return;

    view->fullscreen = !view->fullscreen;
    int dur = wconf::get().anim.focus_ms * 2;

    if (view->fullscreen) {
        view->saved_x = view->tree->node.x;
        view->saved_y = view->tree->node.y;
        anim_move(anims_, &view->tree->node, 0, 0, dur, Easing::EaseOut,
            [this, view]() {
                wlr_xdg_toplevel_set_fullscreen(view->toplevel, true);
                wlr_xdg_toplevel_set_size(view->toplevel, output_w_, output_h_);
            });
        if (view->border_tree) {
            int B = wconf::get().border.width;
            anim_move(anims_, &view->border_tree->node, -B, -B, dur, Easing::EaseOut);
        }
    } else {
        anim_move(anims_, &view->tree->node, (float)view->saved_x, (float)view->saved_y, dur, Easing::EaseOut,
            [this, view]() {
                wlr_xdg_toplevel_set_fullscreen(view->toplevel, false);
            });
        if (view->border_tree) {
            int B = wconf::get().border.width;
            anim_move(anims_, &view->border_tree->node,
                (float)(view->saved_x - B), (float)(view->saved_y - B), dur, Easing::EaseOut);
        }
    }
}

static void spawn_async(const char* cmd, const std::string& sock) {
    pid_t pid = fork();
    if (pid == 0) {
        setsid();
        if (!sock.empty()) {
            setenv("WAYLAND_DISPLAY", sock.c_str(), 1);
        }
        execl("/bin/sh", "sh", "-c", cmd, nullptr);
        _exit(1);
    }
}

void run_action(Compositor* comp, wconf::Action a, const std::string& arg) {
    using wconf::Action;
    switch (a) {
        case Action::Quit:
            std::cout << "[comp] Quitting compositor...\n";
            wl_display_terminate(comp->display_);
            break;
        case Action::Spawn: {
            std::cout << "[comp] Spawning: " << arg << " on " << comp->socket() << "\n";
            spawn_async(arg.c_str(), comp->socket());
            break;
        }
        case Action::ToggleFullscreen:
            comp->toggleFullscreen(comp->focused_view_);
            break;
        case Action::Close:
            if (comp->focused_view_ && comp->focused_view_->toplevel)
                wlr_xdg_toplevel_send_close(comp->focused_view_->toplevel);
            break;
        case Action::FocusNext:
        case Action::FocusPrev:
            break;
        case Action::ToggleFloating:
            if (comp->focused_view_) {
                comp->focused_view_->floating = !comp->focused_view_->floating;
                std::cout << "[comp] floating="
                    << (comp->focused_view_->floating ? "on" : "off") << "\n";
            }
            break;
        case Action::None:
            break;
    }
}

void Compositor::onOutput(wlr_output* out) {
    std::cout << "[comp] output: " << out->name << "\n";

    if (!wlr_output_init_render(out, allocator_, renderer_)) {
        std::cerr << "[comp] wlr_output_init_render failed\n";
        return;
    }

    wlr_output_state st;
    wlr_output_state_init(&st);
    wlr_output_state_set_enabled(&st, true);
    auto* mode = wlr_output_preferred_mode(out);
    if (mode) wlr_output_state_set_mode(&st, mode);
    wlr_output_commit_state(out, &st);
    wlr_output_state_finish(&st);

    auto* ctx = new OutputCtx;
    ctx->comp      = this;
    ctx->output    = out;
    ctx->scene_out = wlr_scene_output_create(scene_, out);
    if (!ctx->scene_out) {
        std::cerr << "[comp] wlr_scene_output_create failed\n";
        delete ctx;
        return;
    }

    auto* lo = wlr_output_layout_add(output_layout_, out, 0, 0);
    wlr_scene_output_layout_add_output(sol_, lo, ctx->scene_out);

    if (out->width > 0 && out->height > 0) {
        output_w_ = out->width;
        output_h_ = out->height;
        if (background_) wlr_scene_rect_set_size(background_, output_w_, output_h_);
        std::cout << "[comp] output size: " << output_w_ << "x" << output_h_ << "\n";
    }

    ctx->frame.notify = [](wl_listener* l, void*) {
        auto* ctx = reinterpret_cast<OutputCtx*>(
            reinterpret_cast<char*>(l) - offsetof(OutputCtx, frame));
        wlr_scene_output_commit(ctx->scene_out, nullptr);
    };
    wl_signal_add(&out->events.frame, &ctx->frame);

    ctx->destroy.notify = [](wl_listener* l, void*) {
        auto* ctx = reinterpret_cast<OutputCtx*>(
            reinterpret_cast<char*>(l) - offsetof(OutputCtx, destroy));
        wl_list_remove(&ctx->frame.link);
        wl_list_remove(&ctx->destroy.link);
        delete ctx;
    };
    wl_signal_add(&out->events.destroy, &ctx->destroy);

    std::cerr << "[comp] output ready: " << out->name << "\n";
}

void Compositor::onInput(wlr_input_device* dev) {
    if (dev->type == WLR_INPUT_DEVICE_KEYBOARD) {
        std::cout << "[comp] keyboard: " << dev->name << "\n";
        if (kbd_) return;

        auto* kb = wlr_keyboard_from_input_device(dev);
        kbd_ = new Keyboard;
        kbd_->comp   = this;
        kbd_->wlr_kb = kb;
        kb->data = this;

        kbd_->ctx = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
        if (!kbd_->ctx) { std::cerr << "[comp] xkb_context_new failed\n"; delete kbd_; kbd_ = nullptr; return; }

        xkb_rule_names rules = {
            .rules   = "evdev",
            .model   = "pc105",
            .layout  = "us,ru",
            .variant = "",
            .options = "grp:alt_shift_toggle"
        };
        kbd_->keymap = xkb_keymap_new_from_names(kbd_->ctx, &rules, XKB_KEYMAP_COMPILE_NO_FLAGS);

        if (!kbd_->keymap) {
            std::cerr << "[comp] xkb_keymap failed\n";
            xkb_context_unref(kbd_->ctx);
            delete kbd_;
            kbd_ = nullptr;
            return;
        }

        kbd_->state = xkb_state_new(kbd_->keymap);
        if (!kbd_->state) {
            std::cerr << "[comp] xkb_state failed\n";
            xkb_keymap_unref(kbd_->keymap);
            xkb_context_unref(kbd_->ctx);
            delete kbd_;
            kbd_ = nullptr;
            return;
        }

        wlr_keyboard_set_keymap(kb, kbd_->keymap);
        wlr_seat_set_keyboard(seat_, kb);

        kbd_->modifiers.notify = cbKeyboardMod;
        wl_signal_add(&kb->events.modifiers, &kbd_->modifiers);

        kbd_->key.notify = cbKeyboardKey;
        wl_signal_add(&kb->events.key, &kbd_->key);

        std::cout << "[comp] keyboard ready\n";
    }

    if (dev->type == WLR_INPUT_DEVICE_POINTER && cursor_) {
        wlr_cursor_attach_input_device(cursor_->cursor, dev);
    }
}

void Compositor::onToplevel(wlr_xdg_toplevel* t) {
    std::cout << "[comp] toplevel: " << (t->app_id ? t->app_id : "?") << std::endl;

    auto* view = viewCreate(t, &scene_->tree);
    views_[t] = view;

    view->commit.notify = [](wl_listener* l, void*) {
        auto* view = reinterpret_cast<View*>(
            reinterpret_cast<char*>(l) - offsetof(View, commit));
        if (view->toplevel && view->toplevel->base->initialized) {
            if (view->grab == View::Grab::Resize) {
                int W = view->toplevel->base->surface->current.width;
                int H = view->toplevel->base->surface->current.height;
                int new_x = view->tree->node.x;
                int new_y = view->tree->node.y;
                if (view->resize.edges & WLR_EDGE_LEFT) {
                    new_x = view->resize.orig_x + (view->resize.win_w - W);
                }
                if (view->resize.edges & WLR_EDGE_TOP) {
                    new_y = view->resize.orig_y + (view->resize.win_h - H);
                }
                wlr_scene_node_set_position(&view->tree->node, new_x, new_y);
            }
            updateViewBorders(view);
            setBorderColor(view, view->is_focused);
            wlr_xdg_toplevel_set_activated(view->toplevel, view->is_focused);
            if (view->is_focused && view->toplevel->base->surface) {
                wlr_seat_keyboard_clear_focus(view->comp->seat_);
                wlr_seat_keyboard_enter(view->comp->seat_, view->toplevel->base->surface,
                                        nullptr, 0, nullptr);
                if (view->comp->kbd_ && view->comp->kbd_->wlr_kb) {
                    wlr_seat_keyboard_send_modifiers(view->comp->seat_,
                                                     &view->comp->kbd_->wlr_kb->modifiers);
                }
            }
        }
    };
    wl_signal_add(&t->base->surface->events.commit, &view->commit);

    view->destroy.notify = [](wl_listener* l, void*) {
        auto* view = reinterpret_cast<View*>(
            reinterpret_cast<char*>(l) - offsetof(View, destroy));
        view->comp->viewDestroy(view);
    };
    wl_signal_add(&t->events.destroy, &view->destroy);

    view->request_move.notify = [](wl_listener* l, void*) {
        auto* view = reinterpret_cast<View*>(
            reinterpret_cast<char*>(l) - offsetof(View, request_move));
        auto* cur = view->comp->cursor_;
        if (cur) {
            view->grab = View::Grab::Move;
            view->move.click_x = (int)cur->cursor->x;
            view->move.click_y = (int)cur->cursor->y;
            view->move.win_x = view->tree->node.x;
            view->move.win_y = view->tree->node.y;
        }
    };
    wl_signal_add(&t->events.request_move, &view->request_move);

    view->request_resize.notify = [](wl_listener* l, void* data) {
        auto* view = reinterpret_cast<View*>(
            reinterpret_cast<char*>(l) - offsetof(View, request_resize));
        auto* ev = static_cast<wlr_xdg_toplevel_resize_event*>(data);
        auto* cur = view->comp->cursor_;
        if (cur) {
            view->grab = View::Grab::Resize;
            view->resize.click_x = (int)cur->cursor->x;
            view->resize.click_y = (int)cur->cursor->y;
            view->resize.orig_x = view->tree->node.x;
            view->resize.orig_y = view->tree->node.y;
            view->resize.win_w = view->surface->surface->current.width;
            view->resize.win_h = view->surface->surface->current.height;
            view->resize.edges = ev->edges;
        }
    };
    wl_signal_add(&t->events.request_resize, &view->request_resize);

    view->request_fullscreen.notify = [](wl_listener* l, void* data) {
        auto* view = reinterpret_cast<View*>(
            reinterpret_cast<char*>(l) - offsetof(View, request_fullscreen));
        std::cerr << "[comp] request_fullscreen\n";
        wlr_xdg_toplevel_set_size(view->toplevel,
                                  view->comp->output_w_, view->comp->output_h_);
        wlr_xdg_toplevel_set_fullscreen(view->toplevel, true);
        wlr_scene_node_set_position(&view->tree->node, 0, 0);
        if (view->border_tree)
            wlr_scene_node_set_position(&view->border_tree->node, 0, 0);
    };
    wl_signal_add(&t->events.request_fullscreen, &view->request_fullscreen);

    if (!focused_view_) {
        setFocusedView(view);
    }
}

void Compositor::cbOutput(wl_listener* l, void* d) {
    auto* c = reinterpret_cast<Compositor*>(
        reinterpret_cast<char*>(l) - offsetof(Compositor, new_output_));
    c->onOutput(static_cast<wlr_output*>(d));
}

void Compositor::cbInput(wl_listener* l, void* d) {
    auto* c = reinterpret_cast<Compositor*>(
        reinterpret_cast<char*>(l) - offsetof(Compositor, new_input_));
    c->onInput(static_cast<wlr_input_device*>(d));
}

void Compositor::cbToplevel(wl_listener* l, void* d) {
    auto* c = reinterpret_cast<Compositor*>(
        reinterpret_cast<char*>(l) - offsetof(Compositor, new_toplevel_));
    c->onToplevel(static_cast<wlr_xdg_toplevel*>(d));
}
void cbKeyboardKey(wl_listener* l, void* data) {
    auto* kbd = reinterpret_cast<Keyboard*>(
        reinterpret_cast<char*>(l) - offsetof(Keyboard, key));
    auto* ev = static_cast<wlr_keyboard_key_event*>(data);
    auto* comp = kbd->comp;

    uint32_t keycode = ev->keycode + 8;
    xkb_state_update_key(kbd->state, keycode,
                         ev->state == WL_KEYBOARD_KEY_STATE_PRESSED ? XKB_KEY_DOWN : XKB_KEY_UP);

    if (comp && ev->state == WL_KEYBOARD_KEY_STATE_PRESSED) {
        uint32_t mods = wlr_keyboard_get_modifiers(kbd->wlr_kb);
        uint32_t clean_mods = mods & (wconf::get().keys.mod_logo |
                                      wconf::get().keys.mod_alt  |
                                      wconf::get().keys.mod_shift |
                                      wconf::get().keys.mod_ctrl);

        const xkb_keysym_t* syms = nullptr;
        int num_syms = xkb_state_key_get_syms(kbd->state, keycode, &syms);

        bool handled = false;
        for (auto& b : wconf::get().binds.list) {
            if (b.mods != clean_mods) continue;

            bool match = (b.key != 0 && b.key == keycode);
            if (!match && b.keysym != 0 && syms) {
                for (int i = 0; i < num_syms; ++i) {
                    if (syms[i] == b.keysym) {
                        match = true;
                        break;
                    }
                }
            }

            if (match) {
                std::string arg = b.arg.empty() ? wconf::get().terminal : b.arg;
                run_action(comp, b.action, arg);
                handled = true;
                break;
            }
        }

        if (handled) {
            return;
        }
    }

    if (comp && comp->focused_view_ && comp->seat_) {
        wlr_seat_keyboard_send_key(comp->seat_, ev->time_msec, ev->keycode, ev->state);
    }
}
void cbKeyboardMod(wl_listener* l, void*) {
    auto* kbd = reinterpret_cast<Keyboard*>(
        reinterpret_cast<char*>(l) - offsetof(Keyboard, modifiers));
    if (kbd && kbd->wlr_kb && kbd->comp && kbd->comp->seat_) {
        wlr_seat_keyboard_send_modifiers(kbd->comp->seat_, &kbd->wlr_kb->modifiers);
    }
}
