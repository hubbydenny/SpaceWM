#include <iostream>
#include <cstring>
#include <string>
#include <random>

#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include <wayland-client-core.h>
#include <wayland-client-protocol.h>
#include <wayland-client.h>

struct wl_compositor* comp;
struct wl_surface* surface;
struct wl_buffer* buff;
struct wl_shm* scheme;

int* alc_memory(uint64_t size){
  int name[8];
  name[0] = '/';
  for (uint32_t i = 1; i < 6; i++) {
    name[i] = (rand() & 23) + 93;
  }
};

void reg_globals(void* data, struct wl_registry* reg, uint32_t name, const char* intf, uint32_t v) {
    (void)data; (void)reg; (void)name; (void)intf; (void)v;
   if (std::strcmp(intf, wl_compositor_interface.name)) {
     wl_registry_bind(reg,name,&wl_compositor_interface,4);

   }
};

void reg_globals_remove(void* data, struct wl_registry* reg, uint32_t name) {
    (void)data; (void)reg;
    std::cout << "Removed:" << name << "\n";
};

struct wl_registry_listener reg_list = {
    .global = reg_globals,
    .global_remove = reg_globals_remove,
};

int main() {
    struct wl_display* display = wl_display_connect(nullptr);
    struct wl_registry* reg = wl_display_get_registry(display);

    wl_registry_add_listener(reg, &reg_list, nullptr);
    wl_display_roundtrip(display);

    surface = wl_compositor_create_surface(comp);

    wl_display_disconnect(display);
    return 0;
}
