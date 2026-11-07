/*
 * united_records.h -- the challenges' and speedruns' records online, the game's side
 * (client/src/united_records.c, SWOS-API): SWOS 2020's galoreapi_v15.php and speedrun
 * swosapi_v3.php, as SWOS 2020 called them (its game.cpp and PHPManager).
 *
 * BLOCKING: each call takes as long as the network does -- from a thread of the caller's own,
 * never a menu's (SWOS United's online-records mod runs them on its worker).
 *
 * Where: PROJECT_RECORDS_API (the game's include/project_identity.h), else SWOS 2020's folder on
 * sensiblesoccer.de. The speedrun API builds its SQL from the request (no prepared statements),
 * so only numbers made here and the escaped name are ever sent to it.
 */
#ifndef UNITED_RECORDS_H
#define UNITED_RECORDS_H

#include "swos_types.h"

/* ---- the challenges ---- */

/* Every player's beaten challenges, tallied: how many players have beaten each
 * ("<category>\t<name>" keys) and the ones `me` has, "<category>\t<name>\n" lines. */
typedef struct { char key[200]; int32_t players; } UnitedChallengeCount;
typedef struct {
    char                 *mine;
    UnitedChallengeCount *counts;
    int32_t               numCounts, capCounts;
} UnitedChallengeTally;

/* 1 fetched (*out filled; free it with UnitedRecords_FreeTally), 0 not. */
int32_t UnitedRecords_ChallengeTally(const char *me, UnitedChallengeTally *out);
void    UnitedRecords_FreeTally(UnitedChallengeTally *t);
/* How many players have beaten `key`; and one more who has (a win just sent): 1 when `me` had
 * not had it already. */
int32_t UnitedRecords_TallyCount(const UnitedChallengeTally *t, const char *key);
int32_t UnitedRecords_TallyAddMine(UnitedChallengeTally *t, const char *key);

/* A challenge beaten, as SWOS 2020 records it (insert_new_scenario_record). 1 sent. */
int32_t UnitedRecords_ChallengeWon(const char *user, const char *scenario, const char *category);

/* ---- the speedruns ---- */

typedef struct {
    const char *user;
    int32_t     style;          /* SWOS 2020's game_style: 0 PC DOS, 1 AMIGA, 3 AMIGA 60 FPS, 4 XBOX */
    int32_t     mode;           /* its game_mode: the target, 0..7 */
    int32_t     difficulty;     /* 0 BEGINNER .. 4 MASTER */
    int32_t     done, frames, ms;
} UnitedSpeedrun;

/* The run into the player's record (view_rows_v2, sync_row_v3), then its line for the result
 * screen into line[cap] ("WORLD RANK 3   BEST 2.21 NAME"). 1 sent. */
int32_t UnitedRecords_SpeedrunRun(const UnitedSpeedrun *run, char *line, uint32_t cap);

/* SWOS 2020's difficulty from the teams' skill points (yours - theirs), and its game_style from
 * OpenSWOS's style name ("AMIGA", "DOS", "XBOX"). */
int32_t UnitedRecords_Difficulty(long yours, long theirs);
int32_t UnitedRecords_Style(const char *name);

#endif /* UNITED_RECORDS_H */
