#include "WaylandClient.hpp"
#include <xkbcommon/xkbcommon.h>
#include <cstdlib>

WaylandClient::WaylandClient() {}

WaylandClient::~WaylandClient() {
    if (m_xdg_toplevel) xdg_toplevel_destroy(m_xdg_toplevel);
    if (m_xdg_surface)  xdg_surface_destroy(m_xdg_surface);
    if (m_xdg_wm_base)  xdg_wm_base_destroy(m_xdg_wm_base);
    if (m_keyboard)     wl_keyboard_destroy(m_keyboard);
    if (m_seat)         wl_seat_destroy(m_seat);
    if (m_buffer)       wl_buffer_destroy(m_buffer);
    if (m_surface)      wl_surface_destroy(m_surface);
    if (m_shm)          wl_shm_destroy(m_shm);
    if (m_compositor)   wl_compositor_destroy(m_compositor);
    if (m_registry)     wl_registry_destroy(m_registry);
    if (m_display)      wl_display_disconnect(m_display);
    if (m_pixels)       ::munmap(m_pixels, static_cast<std::size_t>(m_width) * m_height * 4);
    if (m_state)        xkb_state_unref(m_state);
    if (m_keymap)       xkb_keymap_unref(m_keymap);
    if (m_xkb_context)  xkb_context_unref(m_xkb_context);
}

void WaylandClient::setConfig(Config* cfg) { m_cfg = cfg; }

bool WaylandClient::init() {
    m_display = wl_display_connect(nullptr);
    if (!m_display) { std::cerr << "Cannot connect to Wayland display\n"; return false; }
    m_registry = wl_display_get_registry(m_display);
    wl_registry_add_listener(m_registry, &s_registry_listener, this);
    wl_display_roundtrip(m_display);
    wl_display_roundtrip(m_display);
    if (!m_compositor) { std::cerr << "No compositor\n"; return false; }
    if (!m_shm)        { std::cerr << "No shm\n";        return false; }
    if (!m_xdg_wm_base){ std::cerr << "No xdg_wm_base\n"; return false; }
    return true;
}

void WaylandClient::create_surface() {
    m_surface = wl_compositor_create_surface(m_compositor);

    m_xdg_surface  = xdg_wm_base_get_xdg_surface(m_xdg_wm_base, m_surface);
    xdg_surface_add_listener(m_xdg_surface, &s_xdg_surface_listener, this);

    m_xdg_toplevel = xdg_surface_get_toplevel(m_xdg_surface);
    xdg_toplevel_add_listener(m_xdg_toplevel, &s_xdg_toplevel_listener, this);
    xdg_toplevel_set_title(m_xdg_toplevel, "spacewc");
    xdg_toplevel_set_app_id(m_xdg_toplevel, "spacewc");

    wl_surface_commit(m_surface);
    wl_display_roundtrip(m_display);

    create_shm_buffer();
    std::fill_n(m_pixels, m_width * m_height, 0x00FF0000);
    wl_surface_attach(m_surface, m_buffer, 0, 0);
    wl_surface_damage(m_surface, 0, 0, m_width, m_height);
    wl_surface_commit(m_surface);
}

void WaylandClient::run() {
    std::cout << "[INFO] Running event loop\n";
    while (wl_display_dispatch(m_display) != -1) {}
}

void WaylandClient::registry_global(void* data, wl_registry* reg,
                                    uint32_t name, const char* interface,
                                    uint32_t version) {
    auto* self = static_cast<WaylandClient*>(data);
    (void)version;
    if (std::strcmp(interface, wl_compositor_interface.name) == 0) {
        self->m_compositor = static_cast<wl_compositor*>(
            wl_registry_bind(reg, name, &wl_compositor_interface, 4));
    } else if (std::strcmp(interface, wl_shm_interface.name) == 0) {
        self->m_shm = static_cast<wl_shm*>(
            wl_registry_bind(reg, name, &wl_shm_interface, 1));
    } else if (std::strcmp(interface, wl_seat_interface.name) == 0) {
        self->m_seat = static_cast<wl_seat*>(
            wl_registry_bind(reg, name, &wl_seat_interface, 5));
        wl_seat_add_listener(self->m_seat, &s_seat_listener, self);
    } else if (std::strcmp(interface, xdg_wm_base_interface.name) == 0) {
        self->m_xdg_wm_base = static_cast<xdg_wm_base*>(
            wl_registry_bind(reg, name, &xdg_wm_base_interface, 1));
        xdg_wm_base_add_listener(self->m_xdg_wm_base, &s_xdg_wm_base_listener, self);
    }
}

void WaylandClient::registry_global_remove(void*, wl_registry*, uint32_t) {}

void WaylandClient::seat_capabilities(void* data, wl_seat* seat, uint32_t caps) {
    auto* self = static_cast<WaylandClient*>(data);
    if (caps & WL_SEAT_CAPABILITY_KEYBOARD) {
        self->m_keyboard = wl_seat_get_keyboard(seat);
        wl_keyboard_add_listener(self->m_keyboard, &s_keyboard_listener, self);
        self->m_xkb_context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
        self->m_keymap = xkb_keymap_new_from_names(self->m_xkb_context,
                                                   nullptr, XKB_KEYMAP_COMPILE_NO_FLAGS);
        self->m_state = xkb_state_new(self->m_keymap);
    }
}

void WaylandClient::seat_name(void*, wl_seat*, const char*) {}

void WaylandClient::xdg_wm_base_ping(void*, xdg_wm_base* base, uint32_t serial) {
    xdg_wm_base_pong(base, serial);
}

void WaylandClient::xdg_surface_configure(void* data, xdg_surface* surface, uint32_t serial) {
    auto* self = static_cast<WaylandClient*>(data);
    xdg_surface_ack_configure(surface, serial);
    self->m_configured = true;
}

void WaylandClient::xdg_toplevel_configure(void*, xdg_toplevel*, int32_t, int32_t, wl_array*) {}

void WaylandClient::xdg_toplevel_close(void* data, xdg_toplevel*) {
    auto* self = static_cast<WaylandClient*>(data);
    wl_display_disconnect(self->m_display);
    self->m_display = nullptr;
}

void WaylandClient::xdg_toplevel_configure_bounds(void*, xdg_toplevel*, int32_t, int32_t) {}
void WaylandClient::xdg_toplevel_wm_capabilities(void*, xdg_toplevel*, wl_array*) {}

static std::string key_sym_to_name(xkb_keysym_t ks) {
    char buf[64];
    if (xkb_keysym_get_name(ks, buf, sizeof(buf)) > 0) return std::string(buf);
    return {};
}

void WaylandClient::keyboard_keymap(void*, wl_keyboard*, uint32_t, int32_t, uint32_t) {}
void WaylandClient::keyboard_enter(void*, wl_keyboard*, uint32_t, wl_surface*, wl_array*) {}
void WaylandClient::keyboard_leave(void*, wl_keyboard*, uint32_t, wl_surface*) {}
void WaylandClient::keyboard_modifiers(void* data, wl_keyboard*,
                                       uint32_t, uint32_t mods_depressed,
                                       uint32_t, uint32_t, uint32_t) {
    auto* self = static_cast<WaylandClient*>(data);
    self->m_mods = mods_depressed;
}
void WaylandClient::keyboard_repeat_info(void*, wl_keyboard*, int32_t, int32_t) {}

void WaylandClient::keyboard_key(void* data, wl_keyboard*,
                                 uint32_t, uint32_t, uint32_t key,
                                 uint32_t state) {
    if (state != WL_KEYBOARD_KEY_STATE_PRESSED) return;
    auto* self = static_cast<WaylandClient*>(data);
    if (!self->m_state) return;

    xkb_keysym_t ks = xkb_state_key_get_one_sym(self->m_state, key + 8);
    char buf[64];
    if (xkb_keysym_get_name(ks, buf, sizeof(buf)) <= 0) return;
    std::string name(buf);

    const bool ctrl  = (self->m_mods & 4) != 0;
    const bool shift = (self->m_mods & 1) != 0;
    const bool alt   = (self->m_mods & 8) != 0;

    if (ctrl || shift || alt) {
        std::string combo;
        if (ctrl)  combo += "ctrl+";
        if (shift) combo += "shift+";
        if (alt)   combo += "alt+";
        for (char& c : name) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        combo += name;
        if (self->m_cfg) self->m_cfg->handleKey(combo);
    } else {
        if (self->m_cfg) self->m_cfg->handleKey(name);
    }
}

WaylandClient::UniqueFd WaylandClient::create_shm_fd(std::size_t size) {
    std::string name = "/spacewc-";
    for (int i = 0; i < 6; ++i) name += static_cast<char>((std::rand() & 0x23) + 93);
    int fd = ::shm_open(name.c_str(), O_RDWR | O_CREAT | O_EXCL, S_IWUSR | S_IRUSR | S_IROTH);
    ::shm_unlink(name.c_str());
    ::ftruncate(fd, static_cast<off_t>(size));
    return UniqueFd{fd};
}

void WaylandClient::create_shm_buffer() {
    std::size_t bytes = static_cast<std::size_t>(m_width) * m_height * 4;
    UniqueFd fd = create_shm_fd(bytes);
    m_pixels = reinterpret_cast<int*>(::mmap(nullptr, bytes, PROT_READ | PROT_WRITE,
                                             MAP_SHARED, fd.get(), 0));
    wl_shm_pool* pool = wl_shm_create_pool(m_shm, fd.get(), static_cast<int32_t>(bytes));
    m_buffer = wl_shm_pool_create_buffer(pool, 0, m_width, m_height,
                                         m_width * 4, WL_SHM_FORMAT_XRGB8888);
    wl_shm_pool_destroy(pool);
}
