# sillydoom Documentation — v1.0.0

Technical documentation for sillydoom, a port of id Software's DOOM engine
(linuxdoom-1.10) running on the isilly scripting engine. Targets macOS,
Linux, and Nintendo Switch.

## Architecture

```
┌─────────────────────────────────────────────────────────┐
│                    isilly runtime                       │
│  (scripting engine, windowing, audio, GPU, platform)    │
└────────┬──────────────────────────────────┬─────────────┘
         │                                  │
         ▼                                  ▼
┌─────────────────┐              ┌────────────────────┐
│  main_engine.is │              │  main_switch.is    │
│  (macOS/Linux)  │              │  (Nintendo Switch) │
└────────┬────────┘              └────────┬───────────┘
         │                                │
         ▼                                ▼
┌─────────────────────────────────────────────────────────┐
│                   doom_engine extension                  │
│                                                         │
│  ┌──────────────┐  ┌──────────────┐  ┌───────────────┐ │
│  │ ext_doom_    │  │ doom_isilly  │  │ i_video_      │ │
│  │ engine.c     │  │ .c           │  │ isilly.c      │ │
│  │ (isilly glue)│  │ (lifecycle)  │  │ (video stub)  │ │
│  └──────────────┘  └──────────────┘  └───────────────┘ │
│  ┌──────────────┐  ┌──────────────┐  ┌───────────────┐ │
│  │ i_sound_     │  │ i_net_       │  │ i_system.c    │ │
│  │ isilly.c     │  │ isilly.c     │  │ (timing/mem)  │ │
│  │ (audio)      │  │ (net stubs)  │  │               │ │
│  └──────────────┘  └──────────────┘  └───────────────┘ │
│                                                         │
│  ┌─────────────────────────────────────────────────┐    │
│  │          id DOOM engine (63 .c files)           │    │
│  │  d_main, g_game, r_*, p_*, w_wad, z_zone, ...  │    │
│  └─────────────────────────────────────────────────┘    │
│                                                         │
│  ┌─────────────────────────────────────────────────┐    │
│  │          MUS music (doom_audio/)                 │    │
│  │  mus_parser.c, midi_synth.c, synth_fm.c         │    │
│  └─────────────────────────────────────────────────┘    │
└─────────────────────────────────────────────────────────┘
```

## Chapters

| # | File | Topic |
|---|------|-------|
| 01 | [01-quickstart.md](01-quickstart.md) | Build, run, and deploy |
| 02 | [02-architecture.md](02-architecture.md) | System architecture and data flow |
| 03 | [03-platform-layer.md](03-platform-layer.md) | Platform abstraction (video, audio, input, system) |
| 04 | [04-64bit-porting.md](04-64bit-porting.md) | 64-bit porting guide and fixes applied |
| 05 | [05-switch-port.md](05-switch-port.md) | Nintendo Switch NX homebrew port |
| 06 | [06-audio.md](06-audio.md) | Audio system (SFX + MUS music) |
| 07 | [07-controls.md](07-controls.md) | Input mapping (keyboard, mouse, Joy-Con) |

## Version

- sillydoom v1.0.0
- Based on linuxdoom-1.10 by id Software
- Powered by isilly (SillyState scripting engine)
