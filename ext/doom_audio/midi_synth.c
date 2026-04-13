/* midi_synth.c — Backend dispatcher.
 *
 * Routes a MUS event stream into a chosen synth backend, walks tic time,
 * and accumulates a 16-bit PCM buffer.
 *
 * Backend selection (compile-time):
 *   SILLYDOOM_USE_TSF defined → tsf_backend (high-fidelity SoundFont)
 *   otherwise                 → fm_backend  (tiny built-in FM synth)
 *
 * The backend abstraction lives in doom_audio.h. To add a third backend
 * (OPL3 emulator, GUS patches, etc.), implement synth_backend_t in a new
 * .c, conditionally include it here, and pick it via the same #ifdef.
 */

#include "doom_audio.h"
#include <stdlib.h>
#include <string.h>

#define MUS_TIC_RATE 140

/* Pick the active backend at compile time */
static const synth_backend_t *active_backend(void) {
#if defined(SILLYDOOM_USE_TSF)
    return &tsf_backend;
#else
    return &fm_backend;
#endif
}

int midi_synth_render(const mus_event_t *events, int event_count,
                      int sample_rate, int16_t **out_samples) {
    if (!events || event_count <= 0 || !out_samples || sample_rate <= 0) return 0;

    const synth_backend_t *backend = active_backend();
    if (!backend || !backend->init || !backend->render || !backend->event ||
        !backend->shutdown) return 0;

    /* Bound the render: end at last event's tic + 1 second of release tail */
    uint32_t end_tic = events[event_count - 1].tic;
    int samples_per_tic = sample_rate / MUS_TIC_RATE;
    if (samples_per_tic < 1) samples_per_tic = 1;
    int total_samples = (int)((uint64_t)end_tic * (uint64_t)samples_per_tic) + sample_rate;

    /* Cap at 90 seconds to bound memory */
    int cap = sample_rate * 90;
    if (total_samples > cap) total_samples = cap;
    if (total_samples <= 0) return 0;

    int16_t *buf = (int16_t *)calloc((size_t)total_samples, sizeof(int16_t));
    if (!buf) return 0;

    synth_backend_state_t *st = backend->init(sample_rate);
    if (!st) { free(buf); return 0; }

    int  ev_idx = 0;
    uint32_t cur_tic = 0;
    int  sample_idx = 0;

    while (sample_idx < total_samples) {
        /* Fire all events scheduled at-or-before the current tic */
        while (ev_idx < event_count && events[ev_idx].tic <= cur_tic) {
            backend->event(st, &events[ev_idx]);
            ev_idx++;
        }

        int n = samples_per_tic;
        if (sample_idx + n > total_samples) n = total_samples - sample_idx;
        backend->render(st, &buf[sample_idx], n);
        sample_idx += n;
        cur_tic++;

        if (ev_idx >= event_count) {
            /* Drain remaining release tail until silent or buffer full */
            while (sample_idx < total_samples) {
                if (backend->is_silent && backend->is_silent(st)) break;
                int tail = samples_per_tic;
                if (sample_idx + tail > total_samples) tail = total_samples - sample_idx;
                backend->render(st, &buf[sample_idx], tail);
                sample_idx += tail;
            }
            break;
        }
    }

    backend->shutdown(st);

    /* ── Loop-seam click suppression ──────────────────────────────────
     * The audio engine loops the buffer back to sample 0, which causes
     * an audible pop because the last sample is rarely close to zero.
     * Apply a short linear fade-out to the tail and a matching fade-in
     * to the head — both endpoints become zero, so the loop seam is
     * silence-to-silence. ~30 ms is short enough to be inaudible during
     * sustained music.
     */
    int fade_samples = sample_rate / 33;   /* ~30 ms */
    if (fade_samples * 2 < sample_idx) {
        for (int i = 0; i < fade_samples; i++) {
            float t = (float)i / (float)fade_samples;
            /* Fade in head */
            buf[i] = (int16_t)((float)buf[i] * t);
            /* Fade out tail */
            int tail_idx = sample_idx - 1 - i;
            buf[tail_idx] = (int16_t)((float)buf[tail_idx] * t);
        }
    }

    *out_samples = buf;
    return sample_idx;
}
