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

/* Every player's beaten challenges (get_scenario_completed_ex: a row a win), tallied: each
 * player's wins of each challenge, a challenge known by its "<category>\t<name>" key. */
typedef struct { char user[32]; char key[72]; int32_t wins; } UnitedChallengeWins;
typedef struct {
    UnitedChallengeWins *rows;          /* sorted by key, then user */
    int32_t              numRows, capRows;
    char                 me[32];
} UnitedChallengeTally;

/* 1 fetched (*out filled; free it with UnitedRecords_FreeTally), 0 not. `me` the player (or NULL). */
int32_t UnitedRecords_ChallengeTally(const char *me, UnitedChallengeTally *out);
void    UnitedRecords_FreeTally(UnitedChallengeTally *t);
/* How many players have beaten `key`; how many times `user` has (NULL: the tally's player). */
int32_t UnitedRecords_TallyPlayers(const UnitedChallengeTally *t, const char *key);
int32_t UnitedRecords_TallyWins(const UnitedChallengeTally *t, const char *key, const char *user);
/* The player's beaten challenges: their keys, a line each, into out[cap]. The number. */
int32_t UnitedRecords_TallyMine(const UnitedChallengeTally *t, char *out, uint32_t cap);
/* Who has beaten `key`: "<user>\t<wins>" lines, the most wins first (SWOS 2020's ONLINE STATS). */
int32_t UnitedRecords_TallyWinners(const UnitedChallengeTally *t, const char *key, char *out, uint32_t cap);
/* A category's players by the challenges of it they have beaten, "<user>\t<count>" lines, the
 * most first, then by name (SWOS 2020's rankUsersInCategory: SCENARIO COMPLETION). The number. */
int32_t UnitedRecords_TallyRanking(const UnitedChallengeTally *t, const char *category, char *out, uint32_t cap);
/* A win the player has just sent, counted here too: 1 when it was their first of `key`. */
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
    const char *replayName;     /* the run's replay as the site keeps it ("<user>-...-auto.srp"), */
    const void *replay;         /* uploaded with the record (NULL: none) */
    uint32_t    replaySize;
} UnitedSpeedrun;

/* A run that reached its target into the player's record: its replay uploaded (replays/upload.php),
 * then the record (view_rows_v2, sync_row_v3) naming it -- the last run's, and the best's when it
 * is better -- the replays no record names any more deleted (this game's .srp only), then its line
 * for the result screen into line[cap] ("WORLD RANK 3   BEST 2.21 NAME"). 1 sent. */
int32_t UnitedRecords_SpeedrunRun(const UnitedSpeedrun *run, char *line, uint32_t cap);

/* The boards, as SWOS 2020's screens show them, as text lines into out[cap] (fields tab-separated;
 * a time is its FRAMES at the style's rate, 99999 none; a date YY.MM.DD, from the replay's file
 * name). 1 fetched, 0 not.
 *   Board  (make_leaderboards_v3: a target's five difficulties, 100 places each)
 *          "<difficulty>\t<rank>\t<date>\t<user>\t<played>\t<best frames>\t<best run's replay file>"
 *   Mine   (view_rows_v2: the player's records at a style)
 *          "<target>\t<difficulty>\t<played>\t<best frames>\t<last frames>\t<rank>\t<best run's replay file>
 *          \t<last run's replay file>"
 *   News   (view_news_current_v2: the latest runs, everybody's)
 *          "<date>\t<style>\t<target>\t<difficulty>\t<user>\t<frames>\t<rank, X not a best>\t<the run's replay
 *          file>" */
int32_t UnitedRecords_SpeedrunBoard(int32_t style, int32_t target, char *out, uint32_t cap);
int32_t UnitedRecords_SpeedrunMine(const char *user, int32_t style, char *out, uint32_t cap);
int32_t UnitedRecords_SpeedrunNews(char *out, uint32_t cap);
/* A board's run (the board's last field) from the site's replays/, its bytes into *data (malloc'd:
 * free it). 1 done, 0 not. SWOS United's ("....srp") as it was uploaded; SWOS 2020's ("....RPL"),
 * as it played it from LEADERBOARD (downloadReplayFileEx), a .7z holding that .RPL. */
int32_t UnitedRecords_SpeedrunReplay(const char *file, unsigned char **data, uint32_t *size);

/* ---- SWOS Galore: rating and sharing (galoreapi_v15) ----
 * All / Shared  every shared highlight (newest first) / one player's: "user\tfile\tnote\tdatetime\tscene"
 *               lines (datetime as SWOS 2020 wrote it, "2026-11-21-20-30-05"; scene 0 all of them)
 * Ratings       the rater's ratings: "user\tfile\trate" lines
 * Rate          a rating (0.5..5.0) -- or 0: the rating taken away; the highlight's average follows
 * Share         a highlight shared (its row: note, datetime, scene), and its file uploaded with it
 *               when data is given (highlights/upload_hil.php)
 * Unshare       its row deleted
 * 1 done, 0 not. */
int32_t UnitedRecords_GaloreAll(char *out, uint32_t cap);
int32_t UnitedRecords_GaloreShared(const char *user, char *out, uint32_t cap);
int32_t UnitedRecords_GaloreRatings(const char *rater, char *out, uint32_t cap);
int32_t UnitedRecords_GaloreRate(const char *rater, const char *user, const char *file, double rate);
int32_t UnitedRecords_GaloreShare(const char *user, const char *file, const char *note, const char *datetime,
                                  int32_t scene, const void *data, uint32_t size);
int32_t UnitedRecords_GaloreUnshare(const char *user, const char *file);

/* ---- the shared custom challenges (SWOS 2020's .SCN: a 7z of challenge.ini and two .TEAM) ----
 * List: list_scn_files, "<file>" a line ("ARGENTINA 3-0 BENIN BY BEVEREN.SCN"), by date (newest
 * first) or name. Info: download_scenario_info, "<name>\t<nickname>\t<time>\t<start>\t<et/pen>
 * \t<win>\t<level>" a line, the texts as SWOS 2020 shows them. Download: the file's bytes
 * (*data malloc'd: free it). 1 done, 0 not. */
int32_t UnitedRecords_ScnList(int32_t byName, char *out, uint32_t cap);
int32_t UnitedRecords_ScnInfo(char *out, uint32_t cap);
int32_t UnitedRecords_ScnDownload(const char *file, unsigned char **data, uint32_t *size);

/* ---- SWOS Galore: the highlights shared on SWOS 2020's site (reads only) ----
 * Chart: download_all_hil_data_with_rankings (all time) or ..._year_month ("YYYY-MM"):
 * "<rank>\t<user>\t<file>\t<comment>\t<date YY.MM.DD>\t<scene, 0 all>\t<average rate>\t<raters>"
 * a line, best first. Download: a highlight's .HIL (*data malloc'd: free it). 1 done, 0 not. */
int32_t UnitedRecords_GaloreChart(const char *yearMonth, char *out, uint32_t cap);
int32_t UnitedRecords_GaloreDownload(const char *user, const char *file, unsigned char **data, uint32_t *size);

/* SWOS 2020's difficulty from the teams' skill points (yours - theirs), and its game_style from
 * OpenSWOS's style name ("AMIGA", "DOS", "XBOX"). */
int32_t UnitedRecords_Difficulty(long yours, long theirs);
int32_t UnitedRecords_Style(const char *name);

#endif /* UNITED_RECORDS_H */
