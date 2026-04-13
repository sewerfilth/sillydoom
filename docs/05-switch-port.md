# 05 — Nintendo Switch Port

## Build

```bash
export DEVKITPRO=/opt/devkitpro

# Default: WAD loaded from SD card (2MB NRO)
cd switch && make

# Embedded WAD (13MB NRO, self-contained)
cd switch && make EMBED_WAD=1
```

Output: `switch/sillydoom.nro`

## Deployment

```
sdmc:/
└── switch/
    └── sillydoom/
        ├── sillydoom.nro    ← the homebrew app
        ├── DOOM.WAD         ← your WAD file
        ├── .doomrc          ← created by engine (config)
        ├── crash.log        ← created on crash
        └── doom.log         ← created for diagnostics
```

## Architecture

The Switch port bypasses isilly's windowing system (no pwin backend for Switch)
and calls Switch platform functions directly:

| Function | Purpose | Called From |
|----------|---------|-------------|
| `switch_poll()` | `appletMainLoop()` + `padUpdate()` | `poll_callback()` |
| `switch_present_fb()` | Nearest-neighbor upscale → Switch framebuffer | `present_callback()` |
| `switch_key_pressed()` | Button/stick → isilly keycode | `_key_pressed()` |
| `switch_stick_right()` | Right analog stick → mouse turning | `poll_callback()` |

### Entry Point (`doom_ext_bridge.c`)

The Switch NRO uses a custom `main()` that replaces isilly's CLI:

1. Install signal handlers + `atexit(cleanup_on_exit)`
2. `romfsInit()` — mount embedded filesystem
3. `mkdir("sdmc:/switch/sillydoom/")` — create config directory
4. `setenv("HOME", "sdmc:/switch/sillydoom/")` — for DOOM's `.doomrc`
5. `switch_platform_init()` — pad input + center-clamp
6. `switch_platform_init_fb(1280, 720)` — framebuffer for rendering
7. Load and evaluate `main_switch.is`
8. On error: `show_error_and_wait()` with console message

### Rendering

DOOM renders 320x200 palette-indexed → RGBA uint32 → float RGBA.
`switch_present_fb()` scales to fill the display:

```
320x200 source
    │
    ▼ nearest-neighbor scale
1152x720 output (aspect-preserving, centered)
    │
    ▼ 64px black bars on each side
1280x720 Switch display (undocked)
```

Docked mode (1920x1080) uses the same math — `min(scale_x, scale_y)`
preserves aspect ratio with automatic letterboxing.

### Audio

Switch audio uses `audout` (simple PCM streaming):

- Triple-buffered, 1024 frames per buffer (~21ms latency)
- Dedicated audio thread (priority 0x2C)
- `audoutWaitPlayFinish()` blocks until buffer done
- Float → s16 conversion in fill callback
- `audio_engine_update()` handles its own mutex locking

### Crash Handling

Signal handlers catch SIGSEGV, SIGBUS, SIGABRT, SIGFPE:
- Write `crash.log` to SD card with signal name + step number
- Attempt console display with error message
- Wait for + button press, then `_exit()`

`atexit()` handler calls `switch_platform_destroy()` to release framebuffer
on all exit paths (normal, error, crash).

## Compiler Notes

| Issue | Fix |
|-------|-----|
| devkitPro GCC defaults to C23 (`false`/`true` keywords) | `-std=gnu11` |
| `strupr()` conflicts with newlib | Renamed to `doom_strupr()` |
| `PIXEL_FORMAT_RGBA_8888` byte order | Little-endian: `(a<<24)\|(b<<16)\|(g<<8)\|r` |
| romfs is case-sensitive | WAD lowercased in Makefile, script checks both cases |
| `hidSetNpadAnalogStickUseCenterClamp(true)` | Hardware stick calibration |

## Controls

| Button | DOOM Action |
|--------|-------------|
| L-stick | Forward / back / strafe (via WASD mapping) |
| R-stick X | Turn left/right (mouse dx) |
| D-pad | Forward / back / turn (arrow keys) |
| A | Use / Open / Menu select |
| B | Run (KEY_RSHIFT) |
| X | Automap (TAB) |
| Y | Menu confirm ('y') |
| ZR | Fire (KEY_RCTRL) |
| ZL | Use alternate |
| L shoulder | Strafe left (',') |
| R shoulder | Strafe right ('.') |
| + | Menu (ESC) |
| - | Automap (TAB) |
