#!/bin/bash
set -e
setfont -d
cd "$(dirname "$0")"
meson compile -C build
echo "Copying binary"
sudo cp build/spacewc-comp /usr/local/bin/spacewc-comp
sudo chmod +x /usr/local/bin/spacewc-comp

sudo cp build/spacewc /usr/local/bin/spacewc
sudo chmod +x /usr/local/bin/spacewc

echo "Creating waylandsession"
sudo mkdir -p /usr/share/wayland-sessions

sudo tee /usr/share/wayland-sessions/spacewc.desktop > /dev/null << 'DESK_EOF'
[Desktop Entry]
Name=spacewc
Comment=Space Wayland Compositor
Exec=/usr/local/bin/spacewc-comp
Type=Application
DesktopNames=spacewc
DESK_EOF
