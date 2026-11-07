/*
 * united_records.c -- the challenges' and speedruns' records online (include/united_records.h):
 * SWOS 2020's galoreapi_v15.php and speedrun swosapi_v3.php, over HTTPS (https_client.c).
 */
#if defined(SWOS_SDL_BUILD) && defined(SWOS_HTTPS)

#include "united_records.h"
#include "https_client.h"
#include "project_identity.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef PROJECT_RECORDS_API
#define PROJECT_RECORDS_API "https://sensiblesoccer.de/swos2020"
#endif

static int32_t same_name(const char *a, const char *b)     /* (ASCII, any case: the server's
                                                            *  names come in both) */
{
    for (;; a++, b++) {
        char x = *a, y = *b;
        if (x >= 'a' && x <= 'z') x = (char)(x - 32);
        if (y >= 'a' && y <= 'z') y = (char)(y - 32);
        if (x != y) return 0;
        if (!x) return 1;
    }
}

static int32_t get(const char *url, HttpsReply *r)
{
    memset(r, 0, sizeof *r);
    Https_Request(url, NULL, r);
    if (r->status != 200 || !r->body) {
        fprintf(stderr, "[records] %s: %d\n", url, (int)r->status);
        Https_Free(r);
        return 0;
    }
    return 1;
}

/* ---- the challenges ---- */

static int32_t count_at(UnitedChallengeTally *t, const char *key, int32_t add)
{
    for (int32_t i = 0; i < t->numCounts; i++)
        if (strcmp(t->counts[i].key, key) == 0)
            return add ? (t->counts[i].players += add) : t->counts[i].players;
    if (!add)
        return 0;
    if (t->numCounts == t->capCounts) {
        const int32_t cap = t->capCounts ? t->capCounts * 2 : 256;
        UnitedChallengeCount *more = (UnitedChallengeCount *)realloc(t->counts, (size_t)cap * sizeof *more);
        if (!more)
            return 0;
        t->counts = more;
        t->capCounts = cap;
    }
    snprintf(t->counts[t->numCounts].key, sizeof t->counts[0].key, "%s", key);
    t->counts[t->numCounts].players = add;
    return t->counts[t->numCounts++].players;
}

static void mine_add(UnitedChallengeTally *t, const char *key)
{
    const size_t len = t->mine ? strlen(t->mine) : 0, k = strlen(key);
    char *more = (char *)realloc(t->mine, len + k + 2);
    if (!more)
        return;
    memcpy(more + len, key, k);
    more[len + k] = '\n';
    more[len + k + 1] = 0;
    t->mine = more;
}

static int32_t mine_has(const UnitedChallengeTally *t, const char *key)
{
    const size_t k = strlen(key);
    for (const char *p = t->mine; p && (p = strstr(p, key)) != NULL; p++)
        if ((p == t->mine || p[-1] == '\n') && p[k] == '\n')
            return 1;
    return 0;
}

/* galoreapi get_scenario_completed_ex: "<user>, <scenario>, <category>, 0" a line, every player's */
int32_t UnitedRecords_ChallengeTally(const char *me, UnitedChallengeTally *out)
{
    char url[512];
    HttpsReply r;
    memset(out, 0, sizeof *out);
    snprintf(url, sizeof url, "%s/galoreapi_v15.php?conn_mode=1&option=get_scenario_completed_ex",
             PROJECT_RECORDS_API);
    if (!get(url, &r))
        return 0;
    for (char *line = r.body; line && *line;) {
        char *next = strchr(line, '\n');
        if (next)
            *next++ = 0;
        size_t n = strlen(line);
        while (n && (line[n - 1] == '\r' || line[n - 1] == ' '))
            line[--n] = 0;
        if (n > 3 && strcmp(line + n - 3, ", 0") == 0)
            line[n -= 3] = 0;
        char *first = strstr(line, ", ");           /* user, then the scenario ... */
        char *last = NULL;
        for (char *p = first ? strstr(first + 2, ", ") : NULL; p; p = strstr(p + 2, ", "))
            last = p;                               /* ... and the category after the last ", " */
        if (first && last && last > first) {
            *first = 0;
            *last = 0;
            char key[200];
            snprintf(key, sizeof key, "%s\t%s", last + 2, first + 2);
            count_at(out, key, 1);
            if (me && same_name(line, me) && !mine_has(out, key))
                mine_add(out, key);
        }
        line = next;
    }
    Https_Free(&r);
    if (!out->mine) {
        out->mine = (char *)calloc(1, 1);
        if (!out->mine)
            return 0;
    }
    return 1;
}

void UnitedRecords_FreeTally(UnitedChallengeTally *t)
{
    free(t->mine);
    free(t->counts);
    memset(t, 0, sizeof *t);
}

int32_t UnitedRecords_TallyCount(const UnitedChallengeTally *t, const char *key)
{
    return count_at((UnitedChallengeTally *)t, key, 0);
}

int32_t UnitedRecords_TallyAddMine(UnitedChallengeTally *t, const char *key)
{
    if (mine_has(t, key))
        return 0;
    count_at(t, key, 1);
    mine_add(t, key);
    return 1;
}

int32_t UnitedRecords_ChallengeWon(const char *user, const char *scenario, const char *category)
{
    char url[1024], u[160], s[300], c[300];
    HttpsReply r;
    Https_Escape(user, u, sizeof u);
    Https_Escape(scenario, s, sizeof s);
    Https_Escape(category, c, sizeof c);
    snprintf(url, sizeof url,
             "%s/galoreapi_v15.php?conn_mode=1&option=insert_new_scenario_record&username=%s&scenario=%s&category=%s",
             PROJECT_RECORDS_API, u, s, c);
    if (!get(url, &r))
        return 0;
    Https_Free(&r);
    return 1;
}

/* ---- the speedruns (swosapi_v3) ---- */

typedef struct { int32_t found, played, ranking, bStop, bElaps; double best, current; } RunRow;

/* The player's row for (mode, difficulty) in a view_rows_v2 answer: "user, mode, difficulty,
 * played, best, current, ranking, bestFile, currentFile, bStart, bStop, bElaps, cStart, cStop,
 * cElaps, 0" a line. */
static RunRow find_row(const char *body, int32_t mode, int32_t difficulty)
{
    RunRow row;
    memset(&row, 0, sizeof row);
    for (const char *line = body; line && *line;) {
        const char *next = strchr(line, '\n');
        char buf[512];
        const size_t n = next ? (size_t)(next - line) : strlen(line);
        snprintf(buf, sizeof buf, "%.*s", (int)(n < sizeof buf ? n : sizeof buf - 1), line);
        char *f[16];
        int32_t k = 0;
        for (char *p = buf; p && k < 16; k++) {
            f[k] = p;
            p = strstr(p, ", ");
            if (p) {
                *p = 0;
                p += 2;
            }
        }
        if (k >= 12 && atoi(f[1]) == mode && atoi(f[2]) == difficulty) {
            row.found = 1;
            row.played = atoi(f[3]);
            row.best = atof(f[4]);
            row.current = atof(f[5]);
            row.ranking = atoi(f[6]);
            row.bStop = atoi(f[10]);
            row.bElaps = atoi(f[11]);
            return row;
        }
        line = next ? next + 1 : NULL;
    }
    return row;
}

static RunRow view_row(const char *user, int32_t style, int32_t mode, int32_t difficulty)
{
    char url[512];
    HttpsReply r;
    RunRow row;
    memset(&row, 0, sizeof row);
    snprintf(url, sizeof url, "%s/swosapi_v3.php?conn_mode=1&option=view_rows_v2&username=%s&game_style=%d",
             PROJECT_RECORDS_API, user, (int)style);
    if (get(url, &r)) {
        row = find_row(r.body, mode, difficulty);
        Https_Free(&r);
    }
    return row;
}

int32_t UnitedRecords_SpeedrunRun(const UnitedSpeedrun *run, char *line, uint32_t cap)
{
    char url[1024], user[160];
    HttpsReply r;
    if (line && cap)
        line[0] = 0;
    if (run->mode < 0 || run->mode > 7 || run->difficulty < 0 || run->difficulty > 4)
        return 0;
    Https_Escape(run->user, user, sizeof user);
    const RunRow old = view_row(user, run->style, run->mode, run->difficulty);
    /* the record as SWOS 2020 keeps it: plays, the best and the last time (seconds), and their
     * frames -- from kick-off to the target (start 0) */
    const double now = run->done ? run->ms / 1000.0 : old.current;
    const int32_t better = run->done && (!old.found || old.best <= 0.0 || now < old.best);
    const double best = better ? now : old.best;
    const int32_t bStop = better ? run->frames : old.bStop, bElaps = better ? run->frames : old.bElaps;
    const int32_t cStop = run->done ? run->frames : 0;
    snprintf(url, sizeof url,
             "%s/swosapi_v3.php?conn_mode=1&option=sync_row_v3&username=%s&game_style=%d&game_mode=%d"
             "&difficulty=%d&played=%d&best_record=%.3f&current_record=%.3f&best_replay_filename="
             "&current_replay_filename=&best_record_frame_start=0&best_record_frame_stop=%d"
             "&best_record_frame_elaps=%d&current_record_frame_start=0&current_record_frame_stop=%d"
             "&current_record_frame_elaps=%d",
             PROJECT_RECORDS_API, user, (int)run->style, (int)run->mode, (int)run->difficulty,
             (int)(old.played + 1), best, now, (int)bStop, (int)bElaps, (int)cStop, (int)cStop);
    if (!get(url, &r))
        return 0;
    Https_Free(&r);
    /* the line: the player's rank now, and the board's best */
    const int32_t rank = view_row(user, run->style, run->mode, run->difficulty).ranking;
    char top[64] = "";
    snprintf(url, sizeof url,
             "%s/swosapi_v3.php?conn_mode=1&option=make_leaderboard_v2&game_style=%d&game_mode=%d&difficulty=%d",
             PROJECT_RECORDS_API, (int)run->style, (int)run->mode, (int)run->difficulty);
    if (get(url, &r)) {     /* the first row: "<user>, mode, difficulty, played, <best>, ..." */
        const char *c1 = strstr(r.body, ", ");
        if (c1 && strncmp(r.body, "none,", 5) != 0) {
            char name[32];
            const int32_t nameLen = (int32_t)(c1 - r.body);
            snprintf(name, sizeof name, "%.*s", nameLen < 31 ? nameLen : 31, r.body);
            const char *p = c1;
            for (int32_t k = 0; k < 3 && p; k++)
                p = strstr(p + 2, ", ");
            const double topBest = p ? atof(p + 2) : 0.0;
            for (char *c = name; *c; c++)
                if (*c >= 'a' && *c <= 'z')
                    *c = (char)(*c - 32);
            if (topBest > 0.0)
                snprintf(top, sizeof top, "BEST %.2f %s", topBest, name);
        }
        Https_Free(&r);
    }
    if (line && cap) {
        if (rank > 0)
            snprintf(line, cap, "WORLD RANK %d   %s", (int)rank, top);
        else
            snprintf(line, cap, "%s", top);
    }
    return 1;
}

int32_t UnitedRecords_Difficulty(long yours, long theirs)
{
    const long d = yours - theirs;      /* (SWOS 2020's game.cpp, its choose-the-teams speedrun) */
    return d >= 375 ? 0 : d >= 150 ? 1 : d >= -149 ? 2 : d >= -374 ? 3 : 4;
}

int32_t UnitedRecords_Style(const char *name)
{
    return !strcmp(name, "DOS") ? 0 : !strcmp(name, "XBOX") ? 4 : 1;
}

#endif /* SWOS_SDL_BUILD && SWOS_HTTPS */
