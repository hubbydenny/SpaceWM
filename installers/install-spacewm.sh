#!/bin/bash
# Install spacewm + vcompmgr (compositor) into ~/.local/bin.
#   sh installers/install-spacewm.sh
set -e

SPACEWM_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD_DIR="$SPACEWM_ROOT/build"
DEST_DIR="$HOME/.local/bin"
DEST="$DEST_DIR/spacewm"

# ---- dependencies --------------------------------------------------
install_deps() {
    if command -v pacman >/dev/null 2>&1; then
        local pkgs=(
            xorgproto libx11 libxext libxcomposite libxdamage libxfixes
            libxrender libdrm libxcb xcb-util xcb-util-keysyms xcb-util-cursor
            xcb-util-image xcb-util-renderutil git
        )
        if ! command -v make >/dev/null 2>&1 || ! command -v cc >/dev/null 2>&1; then
            pkgs+=(base-devel)
        fi
        local missing=() p
        for p in "${pkgs[@]}"; do
            pacman -Q "$p" >/dev/null 2>&1 || missing+=("$p")
        done
        if [ ${#missing[@]} -gt 0 ]; then
            echo "==> installing deps: ${missing[*]}"
            sudo pacman -S --needed --noconfirm "${missing[@]}"
        fi
    elif command -v pkg-config >/dev/null 2>&1; then
        if ! pkg-config --exists x11 xext xrender xcomposite xdamage xfixes libdrm; then
            echo "!! missing X libs; install them with your package manager" >&2
            exit 1
        fi
    else
        echo "!! neither pacman nor pkg-config found" >&2
        exit 1
    fi
}

echo "==> checking dependencies..."
install_deps

if [ ! -x "$BUILD_DIR/spacewm" ]; then
    echo "==> binary missing, building..."
    sh "$SPACEWM_ROOT/installers/build.sh"
fi

echo "==> installing $DEST"
mkdir -p "$DEST_DIR"
ln -sf "$BUILD_DIR/spacewm" "$DEST"

# ---- vcompmgr (compositor) ----------------------------------------
VCOMPMGR_URL="https://codeberg.org/hubbydenny/vcompmgr"
VCOMPMGR_DIR="${VCOMPMGR_DIR:-$(dirname "$SPACEWM_ROOT")/vcompmgr}"

if [ ! -x "$VCOMPMGR_DIR/vcompmgr" ]; then
    command -v git >/dev/null 2>&1 || { echo "!! git required to fetch vcompmgr" >&2; exit 1; }
    if [ -d "$VCOMPMGR_DIR/.git" ]; then
        echo "==> updating vcompmgr..."
        git -C "$VCOMPMGR_DIR" pull --ff-only || true
    else
        mkdir -p "$(dirname "$VCOMPMGR_DIR")"
        if [ -d "$VCOMPMGR_DIR" ]; then
            echo "==> using existing $VCOMPMGR_DIR (no .git)"
        else
            echo "==> cloning $VCOMPMGR_URL ..."
            git clone "$VCOMPMGR_URL" "$VCOMPMGR_DIR"
        fi
    fi
    echo "==> building vcompmgr ..."
    make -C "$VCOMPMGR_DIR"
fi

echo "==> installing $DEST_DIR/vcompmgr"
ln -sf "$VCOMPMGR_DIR/vcompmgr" "$DEST_DIR/vcompmgr"

if ! echo "$PATH" | tr ':' '\n' | grep -qx "$DEST_DIR"; then
    if ! grep -q 'HOME/.local/bin' "$HOME/.bashrc" 2>/dev/null; then
        echo 'export PATH="$HOME/.local/bin:$PATH"' >> "$HOME/.bashrc"
        echo "==> added ~/.local/bin to PATH in ~/.bashrc"
    fi
fi

echo "==> done, run spacewm (it starts vcompmgr)"
