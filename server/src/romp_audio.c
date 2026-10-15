/*
 * RO-MP audio stream ala SA-MP (PlayAudioStreamForPlayer) — implementasi.
 * Lihat romp_audio.h untuk desain protokol & API.
 */
#include "romp_audio.h"

/* ---- Helper frame (duplikat kecil dari romp_server.c agar modul mandiri) ---- */
static void aput_u16(uint8_t *b, uint16_t v) { b[0] = v & 0xFF; b[1] = (v >> 8) & 0xFF; }
static void aput_f32(uint8_t *b, float v) {
    uint32_t u; memcpy(&u, &v, 4);
    b[0] = u & 0xFF; b[1] = (u >> 8) & 0xFF;
    b[2] = (u >> 16) & 0xFF; b[3] = (u >> 24) & 0xFF;
}

/* ---------------- API inti ---------------- */

int audio_play(Client *c, const char *url, int usepos,
               float x, float y, float z, float distance, int loop) {
    if (!c || !c->handshaked || c->dead) return 0;
    if (!url || !url[0]) return 0;

    /* Simpan state otoritatif (dipakai romp.getPlayerAudioStream). */
    size_t ul = strlen(url);
    if (ul > AUDIO_URL_MAX) ul = AUDIO_URL_MAX;
    memcpy(c->audio_url, url, ul);
    c->audio_url[ul] = '\0';
    c->audio_playing = 1;
    c->audio_usepos = usepos ? 1 : 0;
    c->audio_x = x; c->audio_y = y; c->audio_z = z;
    c->audio_dist = distance > 0.0f ? distance : 50.0f; /* default ala SA-MP */
    c->audio_loop = loop ? 1 : 0;

    /* Kirim 0x37 AUDIO_PLAY. */
    uint8_t b[2 + AUDIO_URL_MAX + 1 + 4 * 4 + 1];
    b[0] = P_AUDIO_PLAY;
    aput_u16(b + 1, (uint16_t)ul);
    memcpy(b + 3, c->audio_url, ul);
    size_t n = 3 + ul;
    b[n++] = (uint8_t)c->audio_usepos;
    aput_f32(b + n, c->audio_x); n += 4;
    aput_f32(b + n, c->audio_y); n += 4;
    aput_f32(b + n, c->audio_z); n += 4;
    aput_f32(b + n, c->audio_dist); n += 4;
    b[n++] = (uint8_t)c->audio_loop;
    client_send_frame(c, b, n);

    log_msg("[audio] play untuk %s (id %d): %.60s%s (usepos=%d loop=%d)",
            c->name, c->id, c->audio_url,
            ul == AUDIO_URL_MAX ? "..." : "",
            c->audio_usepos, c->audio_loop);
    return 1;
}

int audio_stop(Client *c) {
    if (!c || !c->handshaked || c->dead) return 0;
    if (!c->audio_playing) return 0;
    c->audio_playing = 0;
    c->audio_url[0] = '\0';

    uint8_t b[1];
    b[0] = P_AUDIO_STOP;
    client_send_frame(c, b, 1);

    log_msg("[audio] stop untuk %s (id %d)", c->name, c->id);
    return 1;
}

void audio_on_player_leave(Client *c) {
    if (!c) return;
    c->audio_playing = 0;
    c->audio_url[0] = '\0';
}
