#!/bin/bash
pkill spacewc-comp 2>/dev/null
sleep 0.5
WAYLAND_DISPLAY=wayland-1 XDG_RUNTIME_DIR=/run/user/1000 ./build/spacewc-comp
