#!/bin/bash
#   sh installers/build.sh              # build spacewm + write install-spacewm.sh
#   sh installers/build.sh --clean      # remove build artifacts and installer
set -e

SPACEWM_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SRC_DIR="$SPACEWM_ROOT/src"
BUILD_DIR="${BUILD_DIR:-$SPACEWM_ROOT/build}"
INSTALLER_DIR="$SPACEWM_ROOT/installers"
BIN_NAME="spacewm"
OUT_BIN="$BUILD_DIR/$BIN_NAME"
INSTALLER="$INSTALLER_DIR/install-$BIN_NAME.sh"

CXX="${CXX:-g++}"
CXXFLAGS="${CXXFLAGS:--std=c++20 -Wall -Wextra -O2}"
LIBS="-lxcb -lxcb-keysyms -lxcb-cursor -lxcb-render-util -lxcb-image -lxcb-render"

usage() {
  cat <<EOF
usage: build.sh [options]

options:
  --clean       remove build artifacts and the generated installer
  -h, --help    show this help

builds src/spacewm.cpp into build/spacewm and writes
installers/install-spacewm.sh
EOF
}

write_installer() {
  cat >"$INSTALLER" <<EOF
#!/bin/bash
# Install spacewm + vcompmgr (compositor) into ~/.local/bin.
#   sh installers/install-spacewm.sh
set -e

SPACEWM_ROOT="\$(cd "\$(dirname "\$0")/.." && pwd)"
BUILD_DIR="\$SPACEWM_ROOT/build"
DEST_DIR="\$HOME/.local/bin"
DEST="\$DEST_DIR/$BIN_NAME"

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
        for p in "\${pkgs[@]}"; do
            pacman -Q "\$p" >/dev/null 2>&1 || missing+=("\$p")
        done
        if [ \${#missing[@]} -gt 0 ]; then
            echo "==> installing deps: \${missing[*]}"
            sudo pacman -S --needed --noconfirm "\${missing[@]}"
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

if [ ! -x "\$BUILD_DIR/$BIN_NAME" ]; then
    echo "==> binary missing, building..."
    sh "\$SPACEWM_ROOT/installers/build.sh"
fi

echo "==> installing \$DEST"
mkdir -p "\$DEST_DIR"
ln -sf "\$BUILD_DIR/$BIN_NAME" "\$DEST"

# ---- vcompmgr (compositor) ----------------------------------------
VCOMPMGR_URL="https://codeberg.org/hubbydenny/vcompmgr"
VCOMPMGR_DIR="\${VCOMPMGR_DIR:-\$(dirname "\$SPACEWM_ROOT")/vcompmgr}"

if [ ! -x "\$VCOMPMGR_DIR/vcompmgr" ]; then
    command -v git >/dev/null 2>&1 || { echo "!! git required to fetch vcompmgr" >&2; exit 1; }
    if [ -d "\$VCOMPMGR_DIR/.git" ]; then
        echo "==> updating vcompmgr..."
        git -C "\$VCOMPMGR_DIR" pull --ff-only || true
    else
        mkdir -p "\$(dirname "\$VCOMPMGR_DIR")"
        if [ -d "\$VCOMPMGR_DIR" ]; then
            echo "==> using existing \$VCOMPMGR_DIR (no .git)"
        else
            echo "==> cloning \$VCOMPMGR_URL ..."
            git clone "\$VCOMPMGR_URL" "\$VCOMPMGR_DIR"
        fi
    fi
    echo "==> building vcompmgr ..."
    make -C "\$VCOMPMGR_DIR"
fi

echo "==> installing \$DEST_DIR/vcompmgr"
ln -sf "\$VCOMPMGR_DIR/vcompmgr" "\$DEST_DIR/vcompmgr"

if ! echo "\$PATH" | tr ':' '\n' | grep -qx "\$DEST_DIR"; then
    if ! grep -q 'HOME/.local/bin' "\$HOME/.bashrc" 2>/dev/null; then
        echo 'export PATH="\$HOME/.local/bin:\$PATH"' >> "\$HOME/.bashrc"
        echo "==> added ~/.local/bin to PATH in ~/.bashrc"
    fi
fi

echo "==> done, run $BIN_NAME (it starts vcompmgr)"
EOF
  chmod +x "$INSTALLER"
}

build() {
  echo "==> building $BIN_NAME..."
  mkdir -p "$BUILD_DIR"
  $CXX $CXXFLAGS -o "$OUT_BIN" "$SRC_DIR/$BIN_NAME.cpp" $LIBS
  write_compile_db
}

write_compile_db() {
  local db="$SPACEWM_ROOT/compile_commands.json"
  if cat >"$db" <<EOF
[
  {
    "directory": "$SPACEWM_ROOT",
    "command": "$CXX $CXXFLAGS $SRC_DIR/$BIN_NAME.cpp $LIBS",
    "file": "$SRC_DIR/$BIN_NAME.cpp"
  }
]
EOF
  then
    echo "==> wrote $db"
  else
    echo "!! could not write $db (continuing)"
  fi
}

clean() {
  echo "==> cleaning..."
  rm -rf "$BUILD_DIR"
  rm -f "$INSTALLER"
  rm -f "$SPACEWM_ROOT/compile_commands.json"
  echo "==> done"
}

case "${1:-}" in
-h | --help)
  usage
  ;;
--clean)
  clean
  ;;
"")
  build
  write_installer
  echo "==> done: $OUT_BIN"
  echo "==> installer written: $INSTALLER"
  ;;
*)
  echo "unknown option: $1" >&2
  usage >&2
  exit 1
  ;;
esac
