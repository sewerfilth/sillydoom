# sillydoom

**DOOM on the isilly engine** — id Software's DOOM (linuxdoom-1.10) ported to run as an isilly extension. Plays on macOS, Linux, and Nintendo Switch. (v22.0)

![sillydoom](switch/icon/icon.jpg)

*Create. Destroy. Repeat.*

## What is this?

sillydoom is a port of the original 1993 DOOM engine running on [isilly](https://github.com/sewerfilth/sillystate), a Lua/Python hybrid scripting engine. The id DOOM source code (all 63 .c files) is compiled into a native extension that the isilly runtime loads and executes.

**Features:**
- Full DOOM gameplay (E1M1 through E3M9 for Registered, all of DOOM II)
- SFX audio with 8-channel mixing (DMX format → isilly audio engine)
- MUS music playback via built-in FM synthesizer
- macOS: CoreAudio, Cocoa windowing, mouse capture
- Nintendo Switch: audout audio, Joy-Con/Pro Controller, fullscreen scaling
- Linux: X11/Wayland, ALSA audio
- 64-bit clean (ARM64 + x86_64)

## Requirements

- **isilly** runtime (part of [SillyState](https://github.com/sewerfilth/sillystate)) — not included, proprietary
- **DOOM WAD file** — not included, provide your own
  - `DOOM1.WAD` (Shareware) — free from [doomwiki.org](https://doomwiki.org/wiki/DOOM1.WAD)
  - `DOOM.WAD` (Registered)
  - `DOOM2.WAD` (DOOM II)
- **CMake 3.10+** and a C compiler
- **devkitPro** (for Nintendo Switch builds)

You also need to put the doom engine into ext/doom_engine Linked below

## Building

### macOS
```bash
# Set SILLYSTATE to your sillystate checkout
export SILLYSTATE=/path/to/sillystate

# Build and run
./build.sh --run

# Build .app bundle
./build.sh --mac
```

### Nintendo Switch
```bash
export DEVKITPRO=/opt/devkitpro
export SILLYSTATE=/path/to/sillystate

# Build NRO (WAD loaded from SD card)
./build.sh --switch

# Build NRO with WAD embedded
cd switch && DEVKITPRO=/opt/devkitpro make EMBED_WAD=1
```

Copy `switch/sillydoom.nro` to `sdmc:/switch/sillydoom/` on your SD card.
Place your WAD file in the same directory.

### Linux
```bash
export SILLYSTATE=/path/to/sillystate
cd linux && ./build.sh
```

## WAD File Location

| Platform | Primary (user override) | Fallback (bundled) |
|----------|------------------------|--------------------|
| macOS | `wads/DOOM.WAD` | `assets/DOOM.WAD` |
| Linux | `wads/DOOM.WAD` | `assets/DOOM.WAD` |
| Switch | `sdmc:/switch/sillydoom/DOOM.WAD` | romfs (if EMBED_WAD=1) |

## Switch Controls

| Button | Action |
|--------|--------|
| L-stick | Move + strafe |
| R-stick | Turn |
| D-pad | Move + turn (digital) |
| A | Use / Open / Menu select |
| B | Run |
| X | Automap |
| Y | Menu yes |
| ZR | Fire |
| ZL | Use (alt) |
| L / R | Strafe left/right |
| + | Menu (ESC) |
| - | Automap |

## Project Structure

```
sillydoom/
├── ext/
│   ├── doom_engine/     # id DOOM engine (63 .c files, 64-bit patched)
│   ├── doom_audio/      # MUS parser + FM synth
│   └── CMakeLists.txt   # Builds doom_engine.dylib/.so
├── src/
│   ├── main_engine.is   # macOS/Linux entry script
│   └── main_switch.is   # Switch entry script
├── switch/
│   ├── Makefile          # Switch NRO build
│   └── doom_ext_bridge.c # Switch main() + crash handler
├── linux/
│   └── build.sh          # Linux build script
├── assets/               # Place WAD here
├── wads/                 # User WAD override directory
└── build.sh              # Main build script
```

## Credits

- **DOOM** by id Software (John Carmack, John Romero, et al.) — [source](https://github.com/id-Software/DOOM)
- **isilly / SillyState** by sewerfilth
- **sillydoom** port by sewerfilth

https://www.cuteheart.love

## License

The DOOM engine source code is released under the [DOOM Source Code License](ext/doom_engine/DOOMLIC.TXT).
The sillydoom bridge code, scripts, and build system are MIT licensed.
The isilly/SillyState runtime is proprietary and not included.
