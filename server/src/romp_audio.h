/*
 * RO-MP audio stream ala SA-MP (PlayAudioStreamForPlayer) — server.
 *
 * Server mengirim URL audio ke pemain tertentu; client memutarnya sendiri
 * (WebView/HTML5 audio atau MediaPlayer). Satu stream per pemain, state
 * disimpan di struct Client (audio_*), otoritatif server.
 *
 * Audio bersifat per-pemain seperti di SA-MP: tidak di-broadcast ke pemain
 * lain, dan tidak di-scope virtual world. Stream lama yang masih diputar
 * otomatis diganti (implicit stop) saat URL baru diputar.
 *
 * Protokol (server -> client, lihat PROTOCOL.md):
 *   0x37 AUDIO_PLAY: u16 urllen | char url[urllen]
 *                    | u8 usepos (0=global, 1=posisional)
 *                    | f32 x,y,z (posisi sumber; hanya bila posisional)
 *                    | f32 distance (jarak dengar maks; hanya bila posisional)
 *                    | u8 loop (0=sekali, 1=ulang terus)
 *   0x38 AUDIO_STOP:  (tanpa payload) hentikan stream yang sedang diputar
 */
#pragma once

#include "romp_server.h"

#define AUDIO_URL_MAX 255  /* maks byte URL (u16 urllen) */

/* Putar URL audio untuk pemain. usepos=1 memakai posisi (x,y,z) + distance
 * ala SA-MP (posisi 3D relatif ke pemain; client yang menghitung volume).
 * loop=1 = putar ulang terus sampai di-stop. Kembalikan 1 bila paket
 * terkirim (pemain valid & URL tidak kosong); 0 bila gagal.
 * Stream lama otomatis diganti. */
int audio_play(Client *c, const char *url, int usepos,
               float x, float y, float z, float distance, int loop);

/* Hentikan stream pemain (kirim 0x38 bila memang ada yang diputar).
 * Kembalikan 1 bila ada stream yang dihentikan; 0 bila tidak ada. */
int audio_stop(Client *c);

/* Bersihkan state audio saat pemain keluar (tanpa kirim paket — client
 * sudah putus; untuk NPC tidak ada socket sama sekali). */
void audio_on_player_leave(Client *c);
