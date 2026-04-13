# 06 — Audio System

## Overview

sillydoom replaces DOOM's original Linux OSS audio with the isilly audio engine,
providing cross-platform SFX and music on macOS (CoreAudio), Nintendo Switch
(audout), and Linux (ALSA).

## SFX

### DMX Lump Format

DOOM stores sound effects as WAD lumps with a simple header:

```
Offset  Size  Field
0       2     format (always 3)
2       2     sample_rate (typically 11025)
4       4     num_samples
8       N     unsigned 8-bit PCM samples (128 = silence)
```

### Conversion Pipeline

```
DMX lump (8-bit unsigned, 11025 Hz, mono)
    │
    ├── Linear interpolation resample → 48000 Hz
    ├── uint8 → float32 normalization: (sample - 128) / 128
    │
    ▼
audio_buffer_t (float32, 48000 Hz, mono)
    │
    ▼ cached in g_sfx_cache[lumpnum]
```

Conversion happens once per SFX on first play. Subsequent plays reuse
the cached `audio_buffer_t`.

### Playback

8 mixing channels, matching vanilla DOOM's channel count. DOOM computes
volume (0-127), separation (0-255), and pitch (0-255) per sound:

| DOOM param | isilly mapping |
|-----------|----------------|
| vol 0-127 | gain dB: `20 * log10(vol/127)` |
| sep 0-255 | pan: `(sep - 128) / 128` (-1 to +1) |
| pitch 0-255 | pitch multiplier: `pitch / 128` |

Distance attenuation is set to `AUDIO_ATTEN_NONE` — DOOM already computes
distance-based volume in `S_AdjustSoundParams()`.

## Music

### MUS Format

DOOM music is stored in MUS format (id Software proprietary). The MUS parser
(`mus_parser.c`) converts to a flat event array:

```c
typedef struct {
    uint32_t tic;       // when (1/140 sec ticks)
    uint8_t  kind;      // NOTE_ON, NOTE_OFF, PITCH_BEND, etc.
    uint8_t  channel;   // 0-15
    uint8_t  data1;     // note / controller / bend
    uint8_t  data2;     // velocity / value
} mus_event_t;
```

### FM Synthesis

The built-in FM synth (`synth_fm.c`) renders MUS events to PCM:

- 16 polyphonic voices
- ADSR envelope per voice
- Channel-based timbres (square, saw, triangle, sine, pulse, noise)
- Percussion on channels 9 and 15 (white noise)
- Renders at 48000 Hz mono

### Music Lifecycle

```
S_ChangeMusic()
    │
    ├── I_StopSong()        stop current
    ├── I_UnRegisterSong()  free current buffer/source
    ├── I_RegisterSong()    parse MUS → render PCM → create audio_buffer
    └── I_PlaySong()        create audio_source, set loop, play
```

The entire song is pre-rendered to a float32 buffer at registration time.
Playback loops the buffer using the isilly audio source's loop flag.

## Platform Backends

### macOS — CoreAudio

- Pull model: CoreAudio render callback requests frames
- `audio_engine_update()` called from callback with mutex protection
- Typical latency: 256-1024 frames (~5-21ms)

### Switch — audout

- Push model: dedicated thread fills buffers, queues with `audoutAppendAudioOutBuffer()`
- Triple-buffered (3x 1024 frames = ~64ms total buffer)
- `audoutWaitPlayFinish()` blocks until buffer consumed
- Thread priority 0x2C (high)

### Linux — Planned

ALSA or PulseAudio backend (not yet implemented, falls back to silent).

## Configuration

| Variable | Default | Description |
|----------|---------|-------------|
| `SAMPLE_RATE` | 48000 | Engine sample rate (Hz) |
| `NUM_CHANNELS` | 8 | SFX mixing channels |
| `MAX_SFX_CACHE` | 2048 | Maximum cached SFX buffers |
| `snd_SfxVolume` | 8 | DOOM SFX volume (0-15, from config) |
| `snd_MusicVolume` | 8 | DOOM music volume (0-15, from config) |
