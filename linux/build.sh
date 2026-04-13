#!/bin/bash
# build.sh — Build sillydoom for Linux (all-in-one static binary)
#
# Run this ON a Linux machine (x86_64 or aarch64).
# Requires: gcc, cmake, make, libx11-dev (or wayland-dev), libasound2-dev
#
# Usage:
#   ./build.sh              Build everything
#   ./build.sh --clean      Clean build artifacts
#   ./build.sh --package    Build + create distributable tarball
#
# Output:
#   build/sillydoom         Standalone binary (isilly + doom engine)
#   sillydoom-linux.tar.gz  Distributable package (binary + WAD + scripts)

set -e
cd "$(dirname "$0")"

SILLYSTATE="${SILLYSTATE:-$(cd ../../../../sillystate 2>/dev/null && pwd || cd ../../../sillystate 2>/dev/null && pwd || echo /usr/local/src/sillystate)}"
SILLYDOOM="$(cd .. && pwd)"

log()  { printf "\033[1;36m==>\033[0m %s\n" "$*"; }
ok()   { printf "\033[1;32m✓\033[0m %s\n" "$*"; }
err()  { printf "\033[1;31m✗\033[0m %s\n" "$*" >&2; }

if [ ! -d "$SILLYSTATE/isilly" ]; then
    err "sillystate not found at $SILLYSTATE"
    echo "Set SILLYSTATE env to the sillystate repo root"
    exit 1
fi

ISILLY_ROOT="$SILLYSTATE/isilly"
DOOM_SRC="$SILLYDOOM/ext/doom_engine"
MUS_SRC="$SILLYDOOM/ext/doom_audio"

# ── Step 1: Build isilly for Linux ─────────────────────────────

build_isilly() {
    log "Building isilly for Linux"
    ISILLY_BUILD="$SILLYSTATE/build-linux"
    mkdir -p "$ISILLY_BUILD"
    cd "$ISILLY_BUILD"
    cmake "$SILLYSTATE" \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_C_STANDARD=11 \
        2>&1 | tail -5
    cmake --build . --target isilly_cli -j$(nproc) 2>&1 | tail -5
    cd - >/dev/null

    if [ -f "$ISILLY_BUILD/isilly/isilly_cli" ]; then
        ok "isilly_cli built"
    else
        err "isilly_cli build failed"
        exit 1
    fi
}

# ── Step 2: Build doom_engine as shared library ────────────────

build_doom_engine() {
    log "Building doom_engine.so"
    mkdir -p build

    CFLAGS="-O2 -fPIC -DNORMALUNIX -DLINUX \
            -I$ISILLY_ROOT/sdk/include \
            -I$ISILLY_ROOT/include \
            -I$SILLYSTATE/arm64/include \
            -I$ISILLY_ROOT/src/ext/audio \
            -I$DOOM_SRC -I$MUS_SRC \
            -std=gnu11 \
            -Wno-implicit-function-declaration \
            -Wno-implicit-int \
            -Wno-int-conversion \
            -Wno-incompatible-pointer-types \
            -Wno-pointer-sign \
            -Wno-return-type \
            -Wno-unused-result \
            -Wno-parentheses \
            -Wno-dangling-else \
            -Wno-shift-negative-value \
            -Wno-format"

    # Compile all doom engine .c files
    OBJS=""
    for src in "$DOOM_SRC"/*.c; do
        obj="build/$(basename "$src" .c).o"
        gcc $CFLAGS -c -o "$obj" "$src"
        OBJS="$OBJS $obj"
    done

    # Compile MUS/synth files
    for src in "$MUS_SRC"/mus_parser.c "$MUS_SRC"/midi_synth.c "$MUS_SRC"/synth_fm.c; do
        obj="build/$(basename "$src" .c).o"
        gcc $CFLAGS -c -o "$obj" "$src"
        OBJS="$OBJS $obj"
    done

    # Link as shared library
    gcc -shared -o build/doom_engine.so $OBJS -lm
    ok "build/doom_engine.so ($(stat -c %s build/doom_engine.so 2>/dev/null || stat -f %z build/doom_engine.so) bytes)"
}

# ── Step 3: Assemble the package ───────────────────────────────

assemble() {
    log "Assembling sillydoom package"
    mkdir -p build/package/ext build/package/src build/package/assets

    ISILLY_BUILD="$SILLYSTATE/build-linux"

    # Binary
    cp "$ISILLY_BUILD/isilly/isilly_cli" build/package/sillydoom
    chmod +x build/package/sillydoom

    # Engine extension
    cp build/doom_engine.so build/package/ext/

    # Script
    cp "$SILLYDOOM/src/main_engine.is" build/package/src/

    # Config
    cp "$SILLYDOOM/.isconfig" build/package/

    # WAD (embedded by default)
    cp "$SILLYDOOM/assets/"*.WAD build/package/assets/ 2>/dev/null || \
        cp "$SILLYDOOM/assets/"*.wad build/package/assets/ 2>/dev/null || true

    # wads/ override directory — drop a WAD here to replace the bundled one
    mkdir -p build/package/wads

    # Launcher script
    cat > build/package/run.sh << 'EOF'
#!/bin/bash
cd "$(dirname "$0")"
export LD_LIBRARY_PATH=".:ext:${LD_LIBRARY_PATH:-}"
exec ./sillydoom src/main_engine.is "$@"
EOF
    chmod +x build/package/run.sh

    ok "build/package/ assembled"
}

package_tarball() {
    log "Creating tarball"
    cd build
    tar czf ../sillydoom-linux-$(uname -m).tar.gz -C package .
    cd ..
    SIZE=$(ls -lh sillydoom-linux-*.tar.gz | awk '{print $5}')
    ok "sillydoom-linux-$(uname -m).tar.gz ($SIZE)"
    echo ""
    echo "To run:"
    echo "  tar xzf sillydoom-linux-$(uname -m).tar.gz -C sillydoom"
    echo "  cd sillydoom && ./run.sh"
}

clean_all() {
    rm -rf build *.tar.gz
    ok "Cleaned"
}

# ── Dispatch ───────────────────────────────────────────────────

case "${1:-}" in
    --clean)   clean_all ;;
    --package)
        build_isilly
        build_doom_engine
        assemble
        package_tarball
        ;;
    *)
        build_isilly
        build_doom_engine
        assemble
        echo ""
        echo "Run: cd build/package && ./run.sh"
        echo "Package: $0 --package"
        ;;
esac
