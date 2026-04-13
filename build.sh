#!/bin/bash
# build.sh — sillydoom build pipeline (id DOOM engine)
#
# Usage:
#   ./build.sh             Full build: extension + macOS bundle + Switch NRO
#   ./build.sh --ext       Just rebuild doom_engine dylib (macOS)
#   ./build.sh --mac       Build macOS .app bundle
#   ./build.sh --switch    Build Switch NRO
#   ./build.sh --run       Run sillydoom directly (dev mode)
#   ./build.sh --open      Open the macOS .app bundle
#   ./build.sh --deploy    Deploy NRO to Switch via nxlink
#   ./build.sh --clean     Remove all build artifacts
#   ./build.sh --rebuild-isilly   Rebuild isilly host

set -e
cd "$(dirname "$0")"

SILLYSTATE="${SILLYSTATE:-/Users/allbrancereal/source/sillystate}"
DEVKITPRO="${DEVKITPRO:-/opt/devkitpro}"
ISILLY="$SILLYSTATE/isilly/build/isilly_cli"

NAME="sillydoom"
DIST="dist/${NAME}.app"

log()  { printf "\033[1;36m==>\033[0m %s\n" "$*"; }
ok()   { printf "\033[1;32m✓\033[0m %s\n" "$*"; }
err()  { printf "\033[1;31m✗\033[0m %s\n" "$*" >&2; }

check_isilly_host() {
    if [ ! -f "$ISILLY" ]; then
        err "isilly host not found at $ISILLY"
        exit 1
    fi
}

check_devkit() {
    if [ ! -d "$DEVKITPRO/devkitA64" ]; then
        err "devkitA64 not found"
        exit 1
    fi
}

# ── Build doom_engine extension (macOS dylib) ─────────────────

build_ext() {
    log "Building doom_engine (macOS dylib)"
    if [ ! -d ext/build ]; then
        cmake -S ext -B ext/build -DISILLY_ROOT="$SILLYSTATE/isilly" >/dev/null
    fi
    cmake --build ext/build 2>&1 | grep -E "error:|warning:" || true
    if [ -f ext/doom_engine.dylib ]; then
        ok "ext/doom_engine.dylib ($(stat -f %z ext/doom_engine.dylib) bytes)"
    else
        err "doom_engine build failed"
        exit 1
    fi
}

# ── Build macOS .app bundle ────────────────────────────────────

build_mac() {
    check_isilly_host
    build_ext

    log "Building macOS .app bundle"
    rm -rf "$DIST"
    mkdir -p "$DIST/Contents/MacOS"
    mkdir -p "$DIST/Contents/Frameworks"
    mkdir -p "$DIST/Contents/Resources/src"
    mkdir -p "$DIST/Contents/Resources/ext"
    mkdir -p "$DIST/Contents/Resources/assets"

    cat > "$DIST/Contents/Info.plist" << 'PLIST'
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN"
  "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>CFBundleName</key><string>sillydoom</string>
    <key>CFBundleDisplayName</key><string>sillydoom</string>
    <key>CFBundleIdentifier</key><string>com.sillystate.sillydoom</string>
    <key>CFBundleVersion</key><string>0.2.0</string>
    <key>CFBundleShortVersionString</key><string>0.2</string>
    <key>CFBundleExecutable</key><string>sillydoom</string>
    <key>CFBundlePackageType</key><string>APPL</string>
    <key>CFBundleSignature</key><string>????</string>
    <key>LSMinimumSystemVersion</key><string>13.0</string>
    <key>NSHighResolutionCapable</key><true/>
    <key>NSSupportsAutomaticGraphicsSwitching</key><true/>
    <key>LSApplicationCategoryType</key><string>public.app-category.games</string>
</dict>
</plist>
PLIST

    # isilly_cli + engine dylibs
    cp "$ISILLY" "$DIST/Contents/Frameworks/isilly_cli"
    strip -x "$DIST/Contents/Frameworks/isilly_cli" 2>/dev/null || true
    for dylib in "$SILLYSTATE"/isilly/native/lib/lib*.dylib "$SILLYSTATE"/isilly/native/lib/sillystate.dylib; do
        [ -f "$dylib" ] || continue
        name=$(basename "$dylib")
        real=$(python3 -c "import os; print(os.path.realpath('$dylib'))")
        cp "$real" "$DIST/Contents/Frameworks/$name"
        install_name_tool -id "@executable_path/../Frameworks/$name" \
            "$DIST/Contents/Frameworks/$name" 2>/dev/null || true
    done

    # doom_engine extension
    cp ext/doom_engine.dylib "$DIST/Contents/Resources/ext/"

    # Scripts + config + assets (WAD embedded by default)
    cp src/main_engine.is "$DIST/Contents/Resources/src/"
    cp .isconfig "$DIST/Contents/Resources/"
    cp assets/*.WAD "$DIST/Contents/Resources/assets/" 2>/dev/null || \
        cp assets/*.wad "$DIST/Contents/Resources/assets/" 2>/dev/null || true

    # wads/ override directory — drop a WAD here to replace the bundled one
    mkdir -p "$DIST/Contents/Resources/wads"

    # Launcher script
    cat > "$DIST/Contents/MacOS/sillydoom" << 'LAUNCHER'
#!/bin/bash
DIR="$(cd "$(dirname "$0")" && pwd)"
BUNDLE="$DIR/.."
RESOURCES="$BUNDLE/Resources"
FRAMEWORKS="$BUNDLE/Frameworks"
export DYLD_LIBRARY_PATH="$FRAMEWORKS:${DYLD_LIBRARY_PATH:-}"
cd "$RESOURCES"
exec "$FRAMEWORKS/isilly_cli" "src/main_engine.is" "$@"
LAUNCHER
    chmod +x "$DIST/Contents/MacOS/sillydoom"

    codesign -s - --force --deep "$DIST" 2>/dev/null || true

    SIZE=$(du -sh "$DIST" | cut -f1)
    ok "$DIST  ($SIZE)"
    echo "  Run:  open $DIST"
}

# ── Build Switch NRO ───────────────────────────────────────────

build_switch() {
    check_devkit

    log "Building Switch NRO"
    cd switch
    DEVKITPRO="$DEVKITPRO" make clean 2>/dev/null || true
    DEVKITPRO="$DEVKITPRO" make 2>&1 | tail -5
    if [ -f sillydoom.nro ]; then
        SIZE=$(ls -lh sillydoom.nro | awk '{print $5}')
        ok "switch/sillydoom.nro  ($SIZE)"
        mkdir -p upload/switch/sillydoom
        cp sillydoom.nro upload/switch/sillydoom/
        ok "switch/upload/ ready — copy upload/switch/ to SD card root"
        echo "  Deploy:  $0 --deploy"
    else
        err "Switch NRO build failed"
        exit 1
    fi
    cd ..
}

# ── Run / open / deploy ────────────────────────────────────────

run_dev() {
    check_isilly_host
    build_ext
    log "Running sillydoom (dev mode)"
    exec "$ISILLY" src/main_engine.is
}

open_app() {
    if [ ! -d "$DIST" ]; then
        err "$DIST not found. Run: $0 --mac"
        exit 1
    fi
    open "$DIST"
}

deploy_switch() {
    if [ ! -f switch/sillydoom.nro ]; then
        err "switch/sillydoom.nro not found. Run: $0 --switch"
        exit 1
    fi
    log "Deploying NRO via nxlink"
    "$DEVKITPRO/tools/bin/nxlink" -s switch/sillydoom.nro
}

clean_all() {
    log "Cleaning build artifacts"
    rm -rf ext/build ext/*.dylib dist
    [ -d switch ] && (cd switch && make clean 2>/dev/null || true)
    rm -f switch/*.nro switch/*.nacp switch/*.elf
    rm -rf switch/romfs switch/upload
    ok "Cleaned"
}

rebuild_isilly_host() {
    log "Rebuilding isilly host"
    cd "$SILLYSTATE/isilly/build"
    cmake --build . --target isilly_cli 2>&1 | tail -5
    cd - >/dev/null
    ok "isilly host rebuilt: $ISILLY"
}

# ── Dispatch ───────────────────────────────────────────────────

case "${1:-}" in
    --ext)                   build_ext ;;
    --mac|--macos|--osx)     build_mac ;;
    --switch|--nx)           build_switch ;;
    --linux)                 log "Linux build — run linux/build.sh on a Linux machine"; cd linux && ./build.sh "$@" ;;
    --run)                   run_dev ;;
    --open)                  open_app ;;
    --deploy)                deploy_switch ;;
    --clean)                 clean_all ;;
    --rebuild-isilly)        rebuild_isilly_host ;;
    --help|-h)
        sed -n '2,16p' "$0"
        ;;
    "")
        log "Full build: doom_engine + macOS bundle + Switch NRO"
        build_mac
        if [ -d "$DEVKITPRO/devkitA64" ]; then
            build_switch
        else
            log "Skipping Switch build (devkitPro not available)"
        fi
        echo ""
        ok "All builds complete"
        ;;
    *)
        err "Unknown option: $1"
        echo "Run $0 --help for usage"
        exit 1
        ;;
esac
