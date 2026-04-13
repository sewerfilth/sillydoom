# 03 — Platform Layer

The DOOM engine communicates with the host system through six interface files.
sillydoom replaces the original X11/Linux implementations with isilly-backed
versions that work across macOS, Linux, and Nintendo Switch.

## Interface Files

| File | Original | sillydoom | Purpose |
|------|----------|-----------|---------|
| `i_video.c` | X11/SHM | `i_video_isilly.c` | Framebuffer, palette, input polling |
| `i_sound.c` | Linux OSS | `i_sound_isilly.c` | SFX + music via isilly audio engine |
| `i_net.c` | BSD sockets | `i_net_isilly.c` | Network stubs (single-player) |
| `i_system.c` | Linux | `i_system.c` (patched) | Timing, memory, error handling |

## i_video_isilly.c

### Framebuffer

The DOOM renderer writes 8-bit palette-indexed pixels to `screens[0]` (320x200).
`I_FinishUpdate()` converts to 32-bit RGBA using a 256-entry palette LUT, then
calls the present callback.

```c
void I_FinishUpdate(void) {
    for (int i = 0; i < SCREENWIDTH * SCREENHEIGHT; i++)
        g_rgba[i] = g_palette[screens[0][i]];
    doom_isilly_on_finish_update();  // → present_callback
}
```

### Palette

`I_SetPalette()` applies gamma correction and packs RGB into uint32:

```c
g_palette[i] = (0xFF << 24) | (b << 16) | (g << 8) | r;
```

Format: `0xAABBGGRR` (little-endian RGBA when stored to memory).

### Input

`I_StartTic()` calls `doom_isilly_on_poll()` which triggers the poll callback.
The callback reads platform input (Cocoa keys, Switch pad, etc.) and posts
DOOM events via `doom_isilly_post_event()` into a 64-entry ring buffer.

### Screen Allocation

`I_InitGraphics()` guards against double-allocation — `V_Init()` allocates
`screens[0-3]` as a contiguous block, and `I_InitGraphics()` only allocates
if `screens[0]` is NULL. This prevents `ylookup[]` pointer corruption.

## i_sound_isilly.c

### SFX Pipeline

```
WAD lump (DMX format: 8-bit unsigned, 11025 Hz, mono)
    │
    ▼ convert_doom_sfx()
Float32 audio_buffer_t (48000 Hz, mono, linear interpolation resample)
    │
    ▼ audio_source_create()
isilly audio_source_t (gain, pan, pitch mapped from DOOM vol/sep/pitch)
    │
    ▼ audio_engine_update() [audio thread]
Mixed into output buffer → platform audio device
```

### Music Pipeline

```
WAD lump (MUS format)
    │
    ▼ mus_parse()
mus_event_t array (note on/off, pitch bend, controller changes)
    │
    ▼ midi_synth_render()
int16 PCM mono (FM synthesis at 48000 Hz)
    │
    ▼ audio_buffer_create() + float conversion
Float32 audio_buffer_t → audio_source_t (looping)
```

### Channel Pool

8 mixing channels, matching vanilla DOOM. Each channel holds one
`audio_source_t`. When a new sound starts, a free channel is found
or the oldest channel is stolen.

### Key Constants

DOOM key codes must exactly match `doomdef.h`:

| Constant | Value | DOOM default binding |
|----------|-------|---------------------|
| `KEY_UPARROW` | 0xAD | `key_up` (forward) |
| `KEY_DOWNARROW` | 0xAF | `key_down` (backward) |
| `KEY_LEFTARROW` | 0xAC | `key_left` (turn left) |
| `KEY_RIGHTARROW` | 0xAE | `key_right` (turn right) |
| `KEY_RCTRL` | 0x9D | `key_fire` |
| `KEY_RSHIFT` | 0xB6 | `key_speed` (run) |
| `KEY_RALT` | 0xB8 | `key_strafe` (modifier) |
| `' '` (space) | 0x20 | `key_use` |
| `','` | 0x2C | `key_strafeleft` |
| `'.'` | 0x2E | `key_straferight` |

## i_system.c

### Zone Memory

`I_ZoneBase()` allocates 32 MB via `malloc()`. The zone allocator (`z_zone.c`)
manages all DOOM allocations within this block. 32 MB provides ample headroom
for 64-bit struct sizes (headers grow from 24 to 40 bytes per block).

### Timing

`I_GetTime()` uses `gettimeofday()` and returns time in 1/35th second tics,
matching DOOM's fixed timestep. Works on macOS, Linux, and Switch (via newlib).

### Error Handling

`I_Error()` prints to stderr and calls `exit(-1)`. On Switch, it also
writes to `sdmc:/switch/sillydoom/doom.log` before exiting so errors
can be diagnosed from the SD card.
