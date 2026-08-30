#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>

#include "WaylandClient.hpp"
#include "config.hpp"

int main() { // client testing 
    Config cfg;
    cfg.addBinding("Escape", [] {
        std::cout << "[INFO] Exiting (kb trigger)\n";
        std::exit(0);
    });
    cfg.Log("Program start", LogLevel::Info);

    WaylandClient client;
    if (!client.init()) {
        cfg.Log("Failed to init Wayland client", LogLevel::Error);
        return 1;
    }
    client.setConfig(&cfg);
    client.create_surface();
    client.run();
    return 0;
}
