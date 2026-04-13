/* mus_parser.c — Parse Doom MUS format into a flat event stream.
 *
 * MUS format reference:
 *   Bytes  0-3   "MUS\x1A" magic
 *   Bytes  4-5   score length
 *   Bytes  6-7   score offset (from start of lump)
 *   Bytes  8-9   primary channels
 *   Bytes 10-11  secondary channels
 *   Bytes 12-13  instrument count
 *   Bytes 14-15  reserved
 *   Bytes 16+    instrument list (instrument_count * uint16)
 *   Then         event stream at score offset
 *
 * Event encoding (per event):
 *   Byte 1: bits 4-6 = event kind, bit 7 = "last in group" marker, bits 0-3 = channel
 *   Following bytes: event-specific payload
 *   After last-in-group event, a variable-length delay in MUS-ticks
 *
 * Variable-length delay: each byte contributes (b & 0x7F); top bit = continuation.
 *
 * Event payloads (after the type byte):
 *   0 NOTE_OFF    1B: note (low 7 bits)
 *   1 NOTE_ON     1B: note (bit 7 = "volume follows"), [+1B volume]
 *   2 PITCH_BEND  1B: bend (0-255, 128 = neutral)
 *   3 SYS_EVENT   1B: controller number (no value)
 *   4 CONTROLLER  2B: ctrl number, value
 *   6 END_OF_SCORE — no payload, ends playback
 */

#include "doom_audio.h"
#include <stdlib.h>
#include <string.h>

static uint16_t rd_u16le(const uint8_t *p) {
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

int mus_parse(const uint8_t *data, int size,
              mus_event_t **out_events, int *out_count) {
    if (!data || size < 16 || !out_events || !out_count) return -1;
    if (memcmp(data, "MUS\x1A", 4) != 0) return -1;

    int score_offset = rd_u16le(data + 6);
    if (score_offset < 16 || score_offset >= size) return -1;

    /* Track running volume per channel (MUS only sends new volume on change) */
    uint8_t chan_vol[16];
    memset(chan_vol, 100, sizeof(chan_vol));

    /* First pass: count events to size the output array.
     * Worst case ~1 event per byte; over-allocate slightly. */
    int cap = (size - score_offset) + 16;
    mus_event_t *evs = (mus_event_t *)malloc((size_t)cap * sizeof(mus_event_t));
    if (!evs) return -1;
    int count = 0;
    uint32_t cur_tic = 0;

    int p = score_offset;
    int done = 0;
    while (p < size && !done) {
        uint8_t b = data[p++];
        int kind = (b >> 4) & 0x07;
        int last = (b >> 7) & 1;
        int channel = b & 0x0F;

        mus_event_t ev;
        ev.tic = cur_tic;
        ev.kind = (uint8_t)kind;
        ev.channel = (uint8_t)channel;
        ev.data1 = 0;
        ev.data2 = 0;

        switch (kind) {
            case MUS_EV_NOTE_OFF:
                if (p >= size) goto done;
                ev.data1 = data[p++] & 0x7F;
                ev.data2 = chan_vol[channel];
                break;
            case MUS_EV_NOTE_ON: {
                if (p >= size) goto done;
                uint8_t n = data[p++];
                ev.data1 = n & 0x7F;
                if (n & 0x80) {
                    if (p >= size) goto done;
                    chan_vol[channel] = data[p++] & 0x7F;
                }
                ev.data2 = chan_vol[channel];
                break;
            }
            case MUS_EV_PITCH_BEND:
                if (p >= size) goto done;
                ev.data1 = data[p++];
                break;
            case MUS_EV_SYS_EVENT:
                if (p >= size) goto done;
                ev.data1 = data[p++] & 0x7F;
                break;
            case MUS_EV_CONTROLLER:
                if (p + 1 >= size) goto done;
                ev.data1 = data[p++] & 0x7F;
                ev.data2 = data[p++] & 0x7F;
                break;
            case MUS_EV_END_OF_SCORE:
                done = 1;
                break;
            default:
                /* Unknown / kind 5 / 7 — skip */
                break;
        }

        if (count < cap) evs[count++] = ev;

        if (last && !done) {
            /* Read variable-length delay → advance current tic */
            uint32_t delay = 0;
            while (p < size) {
                uint8_t db = data[p++];
                delay = (delay << 7) | (db & 0x7F);
                if ((db & 0x80) == 0) break;
            }
            cur_tic += delay;
        }
    }
done:
    /* Shrink to actual count — the initial over-allocation can waste ~1 MB */
    if (count > 0 && count < cap) {
        mus_event_t *shrunk = (mus_event_t *)realloc(evs, (size_t)count * sizeof(mus_event_t));
        if (shrunk) evs = shrunk;
    }
    *out_events = evs;
    *out_count = count;
    return 0;
}
