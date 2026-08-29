#!/bin/bash
pkill weston 2>/dev/null
sleep 1

WAYLAND_DISPLAY=wayland-1 XDG_RUNTIME_DIR=/run/user/1000 weston --socket=wayland-weston 2>/dev/null &
WESTON_PID=$!
sleep 3

WAYLAND_DISPLAY=wayland-weston XDG_RUNTIME_DIR=/run/user/1000 ./build/spacewc
kill $WESTON_PID 2>/dev/null
