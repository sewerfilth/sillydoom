/* synth_fm.c — Default tiny polyphonic FM synth backend.
 *
 * Implements the synth_backend_t interface from doom_audio.h. This is the
 * fallback (and only built-in) backend; if SILLYDOOM_USE_TSF is defined,
 * the dispatcher in midi_synth.c will prefer the TSF backend instead.
 *
 * Architecture:
 *   - Voice pool of MAX_VOICES, each holds {note, channel, phase, env_state}
 *   - On NOTE_ON: allocate a free voice (or steal oldest)
 *   - On NOTE_OFF: move voice into release stage
 *   - Per output sample: sum all active voices, apply ADSR + master gain
 *
 * Channels are mapped to distinct timbres (square/saw/triangle/sine/pulse/
 * noise) so different voices in the score sound different. MUS percussion
 * channels (9, 15) use white noise.
 */

#include "doom_audio.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define FM_MAX_VOICES 16

typedef enum { ENV_OFF, ENV_ATTACK, ENV_DECAY, ENV_SUSTAIN, ENV_RELEASE } env_t;

typedef struct {
    int      active;
    uint8_t  channel;
    uint8_t  note;
    uint8_t  velocity;
    float    phase;
    float    phase_inc;
    env_t    env;
    float    env_level;
    uint32_t age;
} fm_voice_t;

struct synth_backend_state {
    int sample_rate;
    int attack_samples;
    int decay_samples;
    int release_samples;
    fm_voice_t voices[FM_MAX_VOICES];
    uint32_t noise_state;
};

static const float ATTACK_TIME  = 0.01f;
static const float DECAY_TIME   = 0.05f;
static const float RELEASE_TIME = 0.10f;
static const float SUSTAIN_LVL  = 0.65f;

/* ── Helpers ───────────────────────────────────────────────────────── */

static float midi_to_freq(int note) {
    return 440.0f * powf(2.0f, (float)(note - 69) / 12.0f);
}

static float fm_noise(synth_backend_state_t *st) {
    st->noise_state ^= st->noise_state << 13;
    st->noise_state ^= st->noise_state >> 17;
    st->noise_state ^= st->noise_state << 5;
    return ((float)(st->noise_state & 0xFFFF) / 32768.0f) - 1.0f;
}

typedef enum {
    TIMBRE_SINE,
    TIMBRE_SQUARE,
    TIMBRE_TRIANGLE,
    TIMBRE_SAW,
    TIMBRE_PULSE25,
    TIMBRE_NOISE,
} timbre_t;

static timbre_t channel_timbre(uint8_t channel) {
    if (channel == 9 || channel == 15) return TIMBRE_NOISE;
    switch (channel & 7) {
        case 0: return TIMBRE_SAW;
        case 1: return TIMBRE_PULSE25;
        case 2: return TIMBRE_TRIANGLE;
        case 3: return TIMBRE_SINE;
        case 4: return TIMBRE_SQUARE;
        case 5: return TIMBRE_SAW;
        case 6: return TIMBRE_TRIANGLE;
        default: return TIMBRE_SQUARE;
    }
}

static int alloc_voice(fm_voice_t *voices) {
    for (int i = 0; i < FM_MAX_VOICES; i++)
        if (!voices[i].active) return i;
    int oldest = 0;
    uint32_t oldest_age = 0;
    for (int i = 0; i < FM_MAX_VOICES; i++) {
        if (voices[i].env == ENV_RELEASE && voices[i].age > oldest_age) {
            oldest = i; oldest_age = voices[i].age;
        }
    }
    if (oldest_age == 0) {
        for (int i = 0; i < FM_MAX_VOICES; i++) {
            if (voices[i].age > oldest_age) {
                oldest = i; oldest_age = voices[i].age;
            }
        }
    }
    return oldest;
}

static float voice_sample(synth_backend_state_t *st, fm_voice_t *v) {
    float out = 0.0f;
    timbre_t tb = channel_timbre(v->channel);
    float p = v->phase;
    float ang = p * 2.0f * 3.14159265f;

    switch (tb) {
        case TIMBRE_SINE:     out = sinf(ang); break;
        case TIMBRE_SQUARE:   out = (p < 0.5f) ? 0.85f : -0.85f; break;
        case TIMBRE_TRIANGLE: out = (p < 0.5f) ? (4.0f * p - 1.0f) : (3.0f - 4.0f * p); break;
        case TIMBRE_SAW:      out = 2.0f * p - 1.0f; break;
        case TIMBRE_PULSE25:  out = (p < 0.25f) ? 0.85f : -0.85f; break;
        case TIMBRE_NOISE:    out = fm_noise(st) * 0.7f; break;
    }
    if (tb != TIMBRE_NOISE) {
        out = out * 0.85f + sinf(ang * 2.0f) * 0.15f;
    }

    v->phase += v->phase_inc;
    if (v->phase >= 1.0f) v->phase -= 1.0f;
    return out;
}

static void env_advance(synth_backend_state_t *st, fm_voice_t *v) {
    switch (v->env) {
        case ENV_ATTACK:
            v->env_level += 1.0f / (float)st->attack_samples;
            if (v->env_level >= 1.0f) {
                v->env_level = 1.0f;
                v->env = ENV_DECAY;
            }
            break;
        case ENV_DECAY:
            v->env_level -= (1.0f - SUSTAIN_LVL) / (float)st->decay_samples;
            if (v->env_level <= SUSTAIN_LVL) {
                v->env_level = SUSTAIN_LVL;
                v->env = ENV_SUSTAIN;
            }
            break;
        case ENV_SUSTAIN:
            break;
        case ENV_RELEASE:
            v->env_level -= SUSTAIN_LVL / (float)st->release_samples;
            if (v->env_level <= 0.0f) {
                v->env_level = 0.0f;
                v->env = ENV_OFF;
                v->active = 0;
            }
            break;
        default: break;
    }
    v->age++;
}

static int16_t mix_one_sample(synth_backend_state_t *st) {
    float mix = 0.0f;
    for (int i = 0; i < FM_MAX_VOICES; i++) {
        fm_voice_t *v = &st->voices[i];
        if (!v->active) continue;
        float s = voice_sample(st, v) * v->env_level *
                  ((float)v->velocity / 127.0f);
        mix += s;
        env_advance(st, v);
    }
    mix *= 0.15f;
    if (mix > 1.0f) mix = 1.0f;
    if (mix < -1.0f) mix = -1.0f;
    return (int16_t)(mix * 32767.0f);
}

/* ── Backend interface ─────────────────────────────────────────────── */

static synth_backend_state_t *fm_init(int sample_rate) {
    synth_backend_state_t *st = (synth_backend_state_t *)calloc(1, sizeof(*st));
    if (!st) return NULL;
    st->sample_rate = sample_rate;
    st->attack_samples  = (int)(ATTACK_TIME  * (float)sample_rate);
    st->decay_samples   = (int)(DECAY_TIME   * (float)sample_rate);
    st->release_samples = (int)(RELEASE_TIME * (float)sample_rate);
    if (st->attack_samples  < 1) st->attack_samples  = 1;
    if (st->decay_samples   < 1) st->decay_samples   = 1;
    if (st->release_samples < 1) st->release_samples = 1;
    st->noise_state = 0xACE1u;
    return st;
}

static void fm_render(synth_backend_state_t *st, int16_t *out, int n_samples) {
    for (int i = 0; i < n_samples; i++) {
        out[i] = mix_one_sample(st);
    }
}

static void fm_event(synth_backend_state_t *st, const mus_event_t *ev) {
    if (ev->kind == MUS_EV_NOTE_ON && ev->data1 > 0) {
        int idx = alloc_voice(st->voices);
        fm_voice_t *v = &st->voices[idx];
        v->active = 1;
        v->channel = ev->channel;
        v->note = ev->data1;
        v->velocity = ev->data2;
        v->phase = 0.0f;
        v->phase_inc = midi_to_freq(ev->data1) / (float)st->sample_rate;
        v->env = ENV_ATTACK;
        v->env_level = 0.0f;
        v->age = 0;
    } else if (ev->kind == MUS_EV_NOTE_OFF) {
        for (int i = 0; i < FM_MAX_VOICES; i++) {
            fm_voice_t *v = &st->voices[i];
            if (v->active && v->channel == ev->channel && v->note == ev->data1 &&
                v->env != ENV_RELEASE && v->env != ENV_OFF) {
                v->env = ENV_RELEASE;
            }
        }
    }
    /* Other event kinds (controller, pitch bend) ignored by the FM backend */
}

static void fm_shutdown(synth_backend_state_t *st) {
    free(st);
}

static int fm_is_silent(synth_backend_state_t *st) {
    for (int i = 0; i < FM_MAX_VOICES; i++)
        if (st->voices[i].active) return 0;
    return 1;
}

const synth_backend_t fm_backend = {
    .name      = "fm",
    .init      = fm_init,
    .render    = fm_render,
    .event     = fm_event,
    .shutdown  = fm_shutdown,
    .is_silent = fm_is_silent,
};
