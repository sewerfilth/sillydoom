/* doom_audio.h — sillydoom MUS music synthesis extension.
 *
 * Architecture (mirrors sillystate's multi-file extension pattern):
 *   mus_parser.c     — Doom MUS lump → linear MIDI-ish event stream
 *   midi_synth.c     — Event stream + tic offsets → 16-bit PCM samples,
 *                       dispatches to one of two synth backends
 *   synth_fm.c       — Default tiny FM/square-wave polyphonic synth
 *   synth_tsf.c      — (optional) TinySoundFont backend, gated on
 *                       SILLYDOOM_USE_TSF — see synth_tsf.c for setup
 *   ext_doom_audio.c — Thin isilly script binding layer
 *
 * Public script API (registered in ext_doom_audio.c):
 *   doomMusicRender(wad_idx, lump_name, out_wav_path, sample_rate)
 *       → 1 on success, 0 on failure
 *       Reads a D_* MUS lump from the WAD, synthesizes it to PCM via the
 *       active synth backend, and writes a mono 16-bit WAV.
 *
 * Default backend (synth_fm) is intentionally tiny — no SoundFont, no GM
 * bank. Music comes out recognisable but synthy. The TSF backend, if
 * compiled in, swaps in a SoundFont-driven sampler for higher fidelity.
 */
#ifndef SILLYDOOM_AUDIO_H
#define SILLYDOOM_AUDIO_H

#include <stdint.h>
#include <stddef.h>

/* ── MUS event stream ──────────────────────────────────────────────── */

typedef enum {
    MUS_EV_NOTE_OFF      = 0,
    MUS_EV_NOTE_ON       = 1,
    MUS_EV_PITCH_BEND    = 2,
    MUS_EV_SYS_EVENT     = 3,
    MUS_EV_CONTROLLER    = 4,
    MUS_EV_END_OF_SCORE  = 6,
} mus_event_kind_t;

typedef struct {
    uint32_t tic;       /* When in MUS-ticks (1/140 sec) the event fires */
    uint8_t  kind;      /* mus_event_kind_t */
    uint8_t  channel;   /* 0..15 */
    uint8_t  data1;     /* note / controller / bend value */
    uint8_t  data2;     /* velocity / controller value */
} mus_event_t;

/* Parse a raw MUS lump into a heap-allocated event array.
 * On success, *out_events is malloc'd (caller frees) and *out_count is set.
 * Returns 0 on success, -1 on parse error. */
int mus_parse(const uint8_t *data, int size,
              mus_event_t **out_events, int *out_count);

/* ── PCM synth ─────────────────────────────────────────────────────── */

/* Render an event stream to a mono 16-bit PCM buffer.
 * sample_rate is typically 11025 or 22050.
 * Caller must free(*out_samples). On success returns sample count, 0 on fail.
 *
 * This is the front door — it dispatches to whichever synth backend is
 * compiled in. Currently the FM backend; the TSF backend (when present)
 * registers itself the same way and the dispatcher picks it via build flag. */
int midi_synth_render(const mus_event_t *events, int event_count,
                      int sample_rate,
                      int16_t **out_samples);

/* ── Synth backend interface ───────────────────────────────────────────
 *
 * A synth backend implements three calls. Backends are stateless across
 * renders — `init` allocates per-render scratch, `render` produces samples,
 * `shutdown` frees.
 *
 * The dispatcher in midi_synth.c picks which backend at compile time:
 *   - default: fm_backend (synth_fm.c)
 *   - if SILLYDOOM_USE_TSF: tsf_backend (synth_tsf.c)
 *
 * To add a new backend (e.g. OPL3 emulator), implement these three
 * functions in your own .c, expose a `synth_backend_t my_backend`, and
 * the dispatcher will route to it via the same #ifdef pattern.
 */

typedef struct synth_backend_state synth_backend_state_t;

typedef struct {
    const char *name;
    /* Allocate per-render state. Returns NULL on failure. */
    synth_backend_state_t *(*init)(int sample_rate);
    /* Render `n_samples` mono int16 samples into `out`, advancing time. */
    void (*render)(synth_backend_state_t *st, int16_t *out, int n_samples);
    /* Process a MUS event at the current synth time. */
    void (*event)(synth_backend_state_t *st, const mus_event_t *ev);
    /* Tear down per-render state. */
    void (*shutdown)(synth_backend_state_t *st);
    /* True when all voices have decayed — used to trim trailing silence. */
    int (*is_silent)(synth_backend_state_t *st);
} synth_backend_t;

/* Default FM backend (always present) */
extern const synth_backend_t fm_backend;

/* TinySoundFont backend (only when SILLYDOOM_USE_TSF is defined) */
#if defined(SILLYDOOM_USE_TSF)
extern const synth_backend_t tsf_backend;
#endif

#endif /* SILLYDOOM_AUDIO_H */
