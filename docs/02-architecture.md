# 02 — Architecture

## Overview

sillydoom is a three-layer system:

1. **isilly runtime** — scripting engine with windowing, audio, GPU, and platform extensions
2. **Entry script** (`main_engine.is` / `main_switch.is`) — finds WAD, creates window, calls `doomRun()`
3. **doom_engine extension** — the complete id DOOM engine compiled as a native isilly extension

The DOOM engine owns the game loop. Once `doomRun()` is called, `D_DoomMain()` takes
control and never returns. All rendering, physics, AI, and game logic runs inside the
engine's `while(1)` loop.

## Data Flow

```
isilly script
    │
    ├── require("doom_engine")    load extension
    ├── framebuffer(320, 200)     create render target (macOS/Linux)
    ├── windowCreate(...)         create display window (macOS/Linux)
    └── doomRun(fb, win, wad)     enter engine ──────────────────┐
                                                                  │
    ┌─────────────────────────────────────────────────────────────┘
    │
    ▼
doom_isilly.c
    ├── setenv("DOOMWADDIR", dir)
    ├── myargc/myargv setup
    └── D_DoomMain()                    ← never returns
         │
         ├── V_Init()                   screen buffers
         ├── M_LoadDefaults()           config from .doomrc
         ├── Z_Init()                   zone memory (32MB)
         ├── W_Init()                   WAD file I/O
         ├── R_Init()                   renderer (textures, sprites, colormaps)
         ├── P_Init()                   playloop (switches, anims, sprites)
         ├── I_Init()                   audio engine (isilly backend)
         ├── S_Init()                   sound system
         └── D_DoomLoop()              ← game loop
              │
              ├── I_StartTic()          poll input ──► poll_callback()
              │                          ├── switch_poll() (Switch)
              │                          ├── pwin_poll_ext() (macOS)
              │                          └── post_key_if_changed() → D_PostEvent()
              │
              ├── G_Ticker()            game logic (1 tic = 1/35 sec)
              │
              ├── D_Display()           render frame → screens[0]
              │
              └── I_FinishUpdate()      present frame ──► present_callback()
                                         ├── palette → RGBA conversion
                                         ├── switch_present_fb() (Switch)
                                         └── pwin_present_ext() (macOS)
```

## Extension Architecture

The doom_engine extension registers a single isilly builtin: `doomRun(fb, win, wad)`.

```
ext_doom_engine.c           isilly extension wrapper
    ├── fn_doom_run()       builtin entry point
    ├── present_callback()  I_FinishUpdate → display
    ├── poll_callback()     I_StartTic → input
    └── key mappings        isilly keys → DOOM keys

doom_isilly.c               engine lifecycle bridge
    ├── doom_isilly_run()   sets up argv, calls D_DoomMain
    └── callbacks           present_fn, poll_fn function pointers

i_video_isilly.c            video interface replacement
    ├── I_InitGraphics()    screen buffer setup
    ├── I_SetPalette()      gamma-corrected palette → RGBA LUT
    ├── I_FinishUpdate()    palette indices → RGBA → present_callback
    └── I_StartTic()        poll_callback → drain event queue

i_sound_isilly.c            audio interface replacement
    ├── I_InitSound()       create isilly audio_engine_t
    ├── I_StartSound()      convert DMX lump → audio_buffer → play
    ├── I_RegisterSong()    parse MUS → FM synth → audio_buffer
    └── 8 mixing channels   audio_source_t per channel

i_net_isilly.c              network stubs (single player only)
i_system.c                  timing (gettimeofday), zone memory (32MB)
```

## File Layout

```
ext/doom_engine/
├── ext_doom_engine.c     isilly extension glue + input/present callbacks
├── doom_isilly.c         engine lifecycle (init, callbacks, run)
├── i_video_isilly.c      video: palette→RGBA, present to window/FB
├── i_sound_isilly.c      audio: DMX→float SFX, MUS→FM music
├── i_net_isilly.c        network stubs
├── i_system.c            timing, memory, error handling
├── [60 id DOOM .c files] original engine (64-bit patched)
└── [45 id DOOM .h files] original headers (64-bit patched)

ext/doom_audio/
├── doom_audio.h          MUS event types, synth backend interface
├── mus_parser.c          MUS lump → event stream
├── midi_synth.c          event stream → PCM (dispatches to backend)
└── synth_fm.c            FM synthesis backend (square/saw/tri waves)
```

## Memory Model

| Region | Size | Purpose |
|--------|------|---------|
| Zone heap | 32 MB | All DOOM allocations (Z_Malloc) |
| screens[0-3] | 256 KB | 320x200 framebuffers |
| SFX cache | Variable | Converted DMX → float32 audio buffers |
| Music buffer | Variable | Pre-rendered MUS → PCM (one song at a time) |
| Audio engine | ~64 KB | isilly mixer, source pool, bus hierarchy |

## Threading Model

| Thread | Purpose | Platform |
|--------|---------|----------|
| Main | Game loop (D_DoomLoop), rendering, input | All |
| Audio | PCM buffer fill → hardware output | All |
| Cocoa event | NSEvent pump (macOS only) | macOS |

The audio thread is managed by the platform backend:
- **macOS**: CoreAudio render callback (pull model)
- **Switch**: audout double-buffer thread (push model)
- **Linux**: ALSA/PulseAudio (planned)
