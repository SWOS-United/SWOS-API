/*
 * united_live2020.h -- SWOS 2020's live matches, from its livestream server on swos.eu
 * (SWOS United's project_identity.h PROJECT_LIVE2020_*): the list of the matches live now, and a
 * match's stream as it is played.
 *
 * The stream is a WebSocket of the server's own messages (SWOS 2020's src/livestream/
 * StreamProtocol.h): a JSON text first -- the match: players, teams, kits, pitch, game style, the
 * picture's size -- then binary messages, the first byte their kind: 0x00 a frame (the camera, the
 * score, the minute, the crowd and every sprite on screen), 0x03 a team's line-up slots, 0x07 the
 * match's JSON again (corrected), and others (penalties, CALL BENCH). This side only carries
 * them; what they mean is the caller's (SWOS United's src/live_cast_fork.inc).
 *
 * Blocking calls, for a worker thread. SDL builds with HTTPS (the native netplay ones).
 */
#ifndef UNITED_LIVE2020_H
#define UNITED_LIVE2020_H

#include <stdint.h>

typedef struct {
    char    streamId[40];
    char    players[2][24];         /* team 1's player, team 2's (the streamer on its side) */
    char    teams[2][24];
    int32_t goals[2];
    int32_t minute;
    int32_t state;                  /* 0 the lobby (no play yet), 1 played, 2 over (the server's matchState) */
} UnitedLive2020;

/* The matches live now (up to `cap`): how many, -1 when the server could not be reached. */
int32_t United_Live2020List(UnitedLive2020 *out, int32_t cap);

typedef struct UnitedLive2020Watch UnitedLive2020Watch;

/* A match's stream, connected (within a few seconds), or NULL. */
UnitedLive2020Watch *United_Live2020Open(const char *streamId);
/* The next message: 0 and *text (1 a JSON text, 0 binary), *data and *len -- valid until the next
 * call -- or -1 when the stream is over (closed, broken, or United_Live2020Stop). */
int32_t United_Live2020Read(UnitedLive2020Watch *w, int32_t *text, const uint8_t **data, uint32_t *len);
/* Ends the connection, from any thread: a Read blocked on it returns -1. */
void    United_Live2020Stop(UnitedLive2020Watch *w);
/* Frees it (after the reading thread is done). */
void    United_Live2020Close(UnitedLive2020Watch *w);

#endif
