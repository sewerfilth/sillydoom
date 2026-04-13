# 01 — Quickstart

## Prerequisites

| Platform | Requirements |
|----------|-------------|
| **macOS** | Apple Silicon or Intel, CMake 3.10+, Xcode CLT |
| **Linux** | GCC, CMake 3.10+, libx11-dev, libasound2-dev |
| **Switch** | devkitPro (devkitA64), libnx |
| **All** | [SillyState](https://github.com/allbrancereal/sillystate) (isilly runtime) |

## Obtain a WAD

sillydoom requires a DOOM WAD file. The engine is **not** included — you must
provide your own:

| WAD | Game | Source |
|-----|------|--------|
| `DOOM1.WAD` | DOOM Shareware | Free — [doomwiki.org](https://doomwiki.org/wiki/DOOM1.WAD) |
| `DOOM.WAD` | DOOM Registered | Purchase from Steam/GOG |
| `DOOM2.WAD` | DOOM II | Purchase from Steam/GOG |

Place the WAD in `assets/` (bundled) or `wads/` (override).

## Build & Run — macOS

```bash
export SILLYSTATE=/path/to/sillystate

# Quick run (builds extension + launches)
./build.sh --run

# Build .app bundle
./build.sh --mac
open dist/sillydoom.app
```

## Build & Run — Linux

```bash
export SILLYSTATE=/path/to/sillystate
cd linux
./build.sh
cd build/package && ./run.sh
```

## Build & Run — Nintendo Switch

```bash
export DEVKITPRO=/opt/devkitpro
export SILLYSTATE=/path/to/sillystate

# Build NRO (2MB — WAD loaded from SD card)
./build.sh --switch

# Or build with WAD embedded (13MB self-contained)
cd switch && DEVKITPRO=/opt/devkitpro make EMBED_WAD=1
```

### Switch setup

1. Copy `switch/sillydoom.nro` to `sdmc:/switch/sillydoom/`
2. Copy your WAD to `sdmc:/switch/sillydoom/DOOM.WAD`
3. Launch from Homebrew Menu

## WAD Search Order

| Priority | macOS / Linux | Nintendo Switch |
|----------|--------------|-----------------|
| 1 | `wads/DOOM2.WAD` | `sdmc:/switch/sillydoom/DOOM2.WAD` |
| 2 | `wads/DOOM.WAD` | `sdmc:/switch/sillydoom/DOOM.WAD` |
| 3 | `DOOM.WAD` (cwd) | `sdmc:/switch/sillydoom/DOOM1.WAD` |
| 4 | `assets/DOOM.WAD` | `romfs:/assets/doom.wad` (if EMBED_WAD) |

DOOM II takes priority when multiple WADs are present.

## Verify

After launching, you should see:
```
DOOM Registered Startup v1.10
V_Init: allocate screens.
M_LoadDefaults: Load system defaults.
Z_Init: Init zone memory allocation daemon.
W_Init: Init WADfiles.
...
```

Then the DOOM title screen with demo playback.
