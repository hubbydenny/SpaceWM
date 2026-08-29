#include "WaylandClient.hpp"
#include "config.hpp"
#include <iostream>

int main() {
    Config cfg;
    cfg.addBinding("Escape", []{
        std::cout << "Exiting (kb trigger)" << std::endl;
        std::exit(0);
    });
    cfg.addBinding("ctrl+s", []{
        std::cout << "Ctrl+S working" << std::endl;
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
    cfg.Log("Program terminated", LogLevel::Info);
    return 0;
}
