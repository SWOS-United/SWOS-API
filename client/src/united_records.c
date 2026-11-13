/*
 * united_records.c -- the challenges' and speedruns' records online (include/united_records.h):
 * SWOS 2020's galoreapi_v15.php and speedrun swosapi_v3.php, over HTTPS (https_client.c).
 */
#if defined(SWOS_SDL_BUILD) && defined(SWOS_HTTPS)

#include "united_records.h"
#include "https_client.h"
#include "project_identity.h"

#include <stdarg.h>
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

static int by_key_user(const void *a, const void *b)
{
    const UnitedChallengeWins *x = (const UnitedChallengeWins *)a, *y = (const UnitedChallengeWins *)b;
    const int k = strcmp(x->key, y->key);
    return k ? k : strcmp(x->user, y->user);
}

static int32_t add_row(UnitedChallengeTally *t, const char *user, const char *key, int32_t wins)
{
    if (t->numRows == t->capRows) {
        const int32_t cap = t->capRows ? t->capRows * 2 : 1024;
        UnitedChallengeWins *more = (UnitedChallengeWins *)realloc(t->rows, (size_t)cap * sizeof *more);
        if (!more)
            return 0;
        t->rows = more;
        t->capRows = cap;
    }
    UnitedChallengeWins *r = &t->rows[t->numRows++];
    snprintf(r->user, sizeof r->user, "%s", user);
    for (char *c = r->user; *c; c++)            /* (the server's names come in both cases) */
        if (*c >= 'a' && *c <= 'z')
            *c = (char)(*c - 32);
    snprintf(r->key, sizeof r->key, "%s", key);
    r->wins = wins;
    return 1;
}

/* Rows of one key are together (sorted): the first, and how many. */
static int32_t key_rows(const UnitedChallengeTally *t, const char *key, int32_t *first)
{
    int32_t lo = 0, hi = t->numRows;
    while (lo < hi) {
        const int32_t mid = (lo + hi) / 2;
        if (strcmp(t->rows[mid].key, key) < 0)
            lo = mid + 1;
        else
            hi = mid;
    }
    int32_t n = 0;
    while (lo + n < t->numRows && !strcmp(t->rows[lo + n].key, key))
        n++;
    *first = lo;
    return n;
}

/* galoreapi get_scenario_completed_ex: "<user>, <scenario>, <category>, 0" a line, a win each */
int32_t UnitedRecords_ChallengeTally(const char *me, UnitedChallengeTally *out)
{
    char url[512];
    HttpsReply r;
    memset(out, 0, sizeof *out);
    snprintf(out->me, sizeof out->me, "%s", me ? me : "");
    for (char *c = out->me; *c; c++)
        if (*c >= 'a' && *c <= 'z')
            *c = (char)(*c - 32);
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
            if (!add_row(out, line, key, 1))
                break;
        }
        line = next;
    }
    Https_Free(&r);
    /* one row a player and challenge, their wins added up */
    qsort(out->rows, (size_t)out->numRows, sizeof *out->rows, by_key_user);
    int32_t w = 0;
    for (int32_t i = 0; i < out->numRows; i++) {
        if (w && !by_key_user(&out->rows[w - 1], &out->rows[i]))
            out->rows[w - 1].wins += out->rows[i].wins;
        else
            out->rows[w++] = out->rows[i];
    }
    out->numRows = w;
    return 1;
}

void UnitedRecords_FreeTally(UnitedChallengeTally *t)
{
    free(t->rows);
    memset(t, 0, sizeof *t);
}

int32_t UnitedRecords_TallyPlayers(const UnitedChallengeTally *t, const char *key)
{
    int32_t first;
    return key_rows(t, key, &first);
}

int32_t UnitedRecords_TallyWins(const UnitedChallengeTally *t, const char *key, const char *user)
{
    int32_t first;
    const int32_t n = key_rows(t, key, &first);
    if (!user)
        user = t->me;
    for (int32_t i = 0; i < n; i++)
        if (same_name(t->rows[first + i].user, user))
            return t->rows[first + i].wins;
    return 0;
}

static void put_line(char **p, uint32_t *left, const char *a, const char *b, int32_t number)
{
    int n = b ? snprintf(*p, *left, "%s\t%s\n", a, b) : snprintf(*p, *left, "%s\t%d\n", a, (int)number);
    if (n > 0 && (uint32_t)n < *left) {
        *p += n;
        *left -= (uint32_t)n;
    }
}

int32_t UnitedRecords_TallyMine(const UnitedChallengeTally *t, char *out, uint32_t cap)
{
    int32_t count = 0;
    char *p = out;
    uint32_t left = cap;
    if (cap)
        out[0] = 0;
    for (int32_t i = 0; i < t->numRows; i++)
        if (t->me[0] && same_name(t->rows[i].user, t->me)) {
            int n = snprintf(p, left, "%s\n", t->rows[i].key);
            if (n > 0 && (uint32_t)n < left) {
                p += n;
                left -= (uint32_t)n;
            }
            count++;
        }
    return count;
}

typedef struct { const char *user; int32_t n; } Ranked;
static int by_count_name(const void *a, const void *b)
{
    const Ranked *x = (const Ranked *)a, *y = (const Ranked *)b;
    return x->n != y->n ? (y->n > x->n ? 1 : -1) : strcmp(x->user, y->user);
}

int32_t UnitedRecords_TallyWinners(const UnitedChallengeTally *t, const char *key, char *out, uint32_t cap)
{
    int32_t first;
    const int32_t n = key_rows(t, key, &first);
    Ranked *r = n ? (Ranked *)malloc((size_t)n * sizeof *r) : NULL;
    char *p = out;
    uint32_t left = cap;
    if (cap)
        out[0] = 0;
    if (!r)
        return 0;
    for (int32_t i = 0; i < n; i++) {
        r[i].user = t->rows[first + i].user;
        r[i].n = t->rows[first + i].wins;
    }
    qsort(r, (size_t)n, sizeof *r, by_count_name);
    for (int32_t i = 0; i < n; i++)
        put_line(&p, &left, r[i].user, NULL, r[i].n);
    free(r);
    return n;
}

static int by_user(const void *a, const void *b)
{
    return strcmp(((const Ranked *)a)->user, ((const Ranked *)b)->user);
}

int32_t UnitedRecords_TallyRanking(const UnitedChallengeTally *t, const char *category, char *out, uint32_t cap)
{
    const size_t cl = strlen(category);
    Ranked *r = t->numRows ? (Ranked *)malloc((size_t)t->numRows * sizeof *r) : NULL;
    int32_t n = 0, users = 0;
    char *p = out;
    uint32_t left = cap;
    if (cap)
        out[0] = 0;
    if (!r)
        return 0;
    /* a row is one player's one challenge: the category's rows, counted a player */
    for (int32_t i = 0; i < t->numRows; i++)
        if (!strncmp(t->rows[i].key, category, cl) && t->rows[i].key[cl] == '\t') {
            r[n].user = t->rows[i].user;
            r[n++].n = 1;
        }
    qsort(r, (size_t)n, sizeof *r, by_user);
    for (int32_t i = 0; i < n; i++) {
        if (users && !strcmp(r[users - 1].user, r[i].user))
            r[users - 1].n++;
        else
            r[users++] = r[i];
    }
    qsort(r, (size_t)users, sizeof *r, by_count_name);
    for (int32_t i = 0; i < users; i++)
        put_line(&p, &left, r[i].user, NULL, r[i].n);
    free(r);
    return users;
}

int32_t UnitedRecords_TallyAddMine(UnitedChallengeTally *t, const char *key)
{
    int32_t first;
    const int32_t n = key_rows(t, key, &first);
    if (!t->me[0])
        return 0;
    for (int32_t i = 0; i < n; i++)
        if (same_name(t->rows[first + i].user, t->me)) {
            t->rows[first + i].wins++;
            return 0;
        }
    if (!add_row(t, t->me, key, 1))
        return 0;
    qsort(t->rows, (size_t)t->numRows, sizeof *t->rows, by_key_user);
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

/* ", "-separated fields of a line, in place: up to max, the count */
static int32_t split_fields(char *line, char **f, int32_t max)
{
    int32_t k = 0;
    for (char *p = line; p && k < max; k++) {
        f[k] = p;
        p = strstr(p, ", ");
        if (p) {
            *p = 0;
            p += 2;
        }
    }
    return k;
}

/* YY.MM.DD from a replay's name: "<user>-2026-1105-123605-auto.rpl" (SWOS 2020's: the 16
 * characters 25 from its end) */
static void file_date(const char *file, char *out, uint32_t cap)
{
    const size_t n = strlen(file);
    if (n < 25) {
        snprintf(out, cap, "-");
        return;
    }
    const char *d = file + n - 25;      /* "2026-1105-123605" */
    snprintf(out, cap, "%.2s.%.2s.%.2s", d + 2, d + 5, d + 7);
}

typedef void (*RowFn)(char **f, int32_t n, char **p, uint32_t *left);

static int32_t each_row(const char *url, int32_t minFields, RowFn fn, char *out, uint32_t cap)
{
    HttpsReply r;
    char *p = out;
    uint32_t left = cap;
    if (cap)
        out[0] = 0;
    if (!get(url, &r))
        return 0;
    for (char *line = r.body; line && *line;) {
        char *next = strchr(line, '\n');
        if (next)
            *next++ = 0;
        size_t len = strlen(line);
        while (len && (line[len - 1] == '\r' || line[len - 1] == ' '))
            line[--len] = 0;
        char *f[20];
        const int32_t n = split_fields(line, f, 20);
        if (n >= minFields)
            fn(f, n, &p, &left);
        line = next;
    }
    Https_Free(&r);
    return 1;
}

static void put_text(char **p, uint32_t *left, const char *fmt, ...) __attribute__((format(printf, 3, 4)));
static void put_text(char **p, uint32_t *left, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    const int n = vsnprintf(*p, *left, fmt, ap);
    va_end(ap);
    if (n > 0 && (uint32_t)n < *left) {
        *p += n;
        *left -= (uint32_t)n;
    }
}

static void upper_name(const char *in, char *out, uint32_t cap)
{
    uint32_t i = 0;
    for (; in[i] && i + 1 < cap; i++)
        out[i] = (char)(in[i] >= 'a' && in[i] <= 'z' ? in[i] - 32 : in[i]);
    out[i] = 0;
}

/* user, mode, difficulty, best, bestFile, rank, played, current, currentFile, bestFrames,
 * currentFrames -- rank 0 an empty place */
static void board_row(char **f, int32_t n, char **p, uint32_t *left)
{
    char date[16], user[40];
    (void)n;
    if (atoi(f[5]) == 0)
        return;
    file_date(f[4], date, sizeof date);
    upper_name(f[0], user, sizeof user);
    put_text(p, left, "%d\t%d\t%s\t%s\t%d\t%d\t%s\n", atoi(f[2]), atoi(f[5]), date, user, atoi(f[6]), atoi(f[9]),
             f[4]);                     /* (the best run's replay: SpeedrunReplay) */
}

int32_t UnitedRecords_SpeedrunBoard(int32_t style, int32_t target, char *out, uint32_t cap)
{
    char url[512];
    snprintf(url, sizeof url, "%s/swosapi_v3.php?conn_mode=1&option=make_leaderboards_v3&game_style=%d&game_mode=%d",
             PROJECT_RECORDS_API, (int)style, (int)target);
    return each_row(url, 11, board_row, out, cap);
}

/* user, mode, difficulty, played, best, current, ranking, bestFile, currentFile, bStart, bStop,
 * bElaps, cStart, cStop, cElaps, 0 */
static void mine_row(char **f, int32_t n, char **p, uint32_t *left)
{
    (void)n;
    put_text(p, left, "%d\t%d\t%d\t%d\t%d\t%d\n", atoi(f[1]), atoi(f[2]), atoi(f[3]), atoi(f[11]), atoi(f[14]),
             atoi(f[6]));
}

int32_t UnitedRecords_SpeedrunMine(const char *user, int32_t style, char *out, uint32_t cap)
{
    char url[512], u[160];
    Https_Escape(user, u, sizeof u);
    snprintf(url, sizeof url, "%s/swosapi_v3.php?conn_mode=1&option=view_rows_v2&username=%s&game_style=%d",
             PROJECT_RECORDS_API, u, (int)style);
    return each_row(url, 15, mine_row, out, cap);
}

/* style, user, mode, difficulty, played, best, current, rank, bestFile, currentFile, frames */
static void news_row(char **f, int32_t n, char **p, uint32_t *left)
{
    char date[16], user[40], rank[12];
    (void)n;
    if (!strcmp(f[1], "none"))
        return;
    file_date(f[9], date, sizeof date);
    upper_name(f[1], user, sizeof user);
    if (!strcmp(f[5], f[6]))            /* (the run is the player's best: its place) */
        snprintf(rank, sizeof rank, "%d", atoi(f[7]));
    else
        snprintf(rank, sizeof rank, "X");
    put_text(p, left, "%s\t%d\t%d\t%d\t%s\t%d\t%s\n", date, atoi(f[0]), atoi(f[2]), atoi(f[3]), user, atoi(f[10]),
             rank);
}

int32_t UnitedRecords_SpeedrunNews(char *out, uint32_t cap)
{
    char url[512];
    snprintf(url, sizeof url, "%s/swosapi_v3.php?conn_mode=1&option=view_news_current_v2", PROJECT_RECORDS_API);
    return each_row(url, 11, news_row, out, cap);
}

/* ---- the shared custom challenges ---- */

static void scn_row(char **f, int32_t n, char **p, uint32_t *left)
{
    (void)n;
    put_text(p, left, "%s\n", f[0]);
}

int32_t UnitedRecords_ScnList(int32_t byName, char *out, uint32_t cap)
{
    char url[512];
    snprintf(url, sizeof url, "%s/galoreapi_v15.php?conn_mode=1&option=list_scn_files&sort_option=%s",
             PROJECT_RECORDS_API, byName ? "name" : "date");
    return each_row(url, 1, scn_row, out, cap);
}

/* "<name>", "<nickname>", "<time>", "<start>", "<et/pen>", "<win>", "<level>", 0 a line: quoted
 * fields, which may hold ", " themselves */
int32_t UnitedRecords_ScnInfo(char *out, uint32_t cap)
{
    char url[512];
    HttpsReply r;
    char *p = out;
    uint32_t left = cap;
    if (cap)
        out[0] = 0;
    snprintf(url, sizeof url, "%s/galoreapi_v15.php?conn_mode=1&option=download_scenario_info", PROJECT_RECORDS_API);
    if (!get(url, &r))
        return 0;
    for (const char *line = r.body; line && *line;) {
        const char *next = strchr(line, '\n');
        const char *end = next ? next : line + strlen(line);
        char f[7][128];
        int32_t k = 0;
        const char *c = line;
        while (k < 7 && c < end) {
            while (c < end && (*c == ' ' || *c == ','))
                c++;
            if (c >= end || *c != '"')
                break;
            const char *q = memchr(c + 1, '"', (size_t)(end - c - 1));
            if (!q)
                break;
            snprintf(f[k], sizeof f[k], "%.*s", (int)(q - c - 1), c + 1);
            for (char *t = f[k]; *t; t++)
                if (*t == '\t')
                    *t = ' ';
            k++;
            c = q + 1;
        }
        if (k == 7)
            put_text(&p, &left, "%s\t%s\t%s\t%s\t%s\t%s\t%s\n", f[0], f[1], f[2], f[3], f[4], f[5], f[6]);
        line = next ? next + 1 : NULL;
    }
    Https_Free(&r);
    return 1;
}

int32_t UnitedRecords_ScnDownload(const char *file, unsigned char **data, uint32_t *size)
{
    char url[640], esc[300];
    HttpsReply r;
    *data = NULL;
    *size = 0;
    Https_Escape(file, esc, sizeof esc);
    snprintf(url, sizeof url, "%s/challenges/custom/%s", PROJECT_RECORDS_API, esc);
    if (!get(url, &r))
        return 0;
    *data = (unsigned char *)malloc(r.length ? r.length : 1);
    if (*data) {
        memcpy(*data, r.body, r.length);
        *size = r.length;
    }
    Https_Free(&r);
    return *data != NULL;
}

/* ---- SWOS Galore ---- */

/* "1, nestoroide, COMBO.HIL, "NICE HEAD COMBO", 2026-03-06-16-46-23, 2, 5.00, 7, 0": the comment
 * quoted (it may hold ", "), the rest bare */
int32_t UnitedRecords_GaloreChart(const char *yearMonth, char *out, uint32_t cap)
{
    char url[512];
    HttpsReply r;
    char *p = out;
    uint32_t left = cap;
    if (cap)
        out[0] = 0;
    if (yearMonth && yearMonth[0])
        snprintf(url, sizeof url,
                 "%s/galoreapi_v15.php?conn_mode=1&option=download_all_hil_data_with_rankings_year_month&year_month=%.7s",
                 PROJECT_RECORDS_API, yearMonth);
    else
        snprintf(url, sizeof url, "%s/galoreapi_v15.php?conn_mode=1&option=download_all_hil_data_with_rankings",
                 PROJECT_RECORDS_API);
    if (!get(url, &r))
        return 0;
    for (char *line = r.body; line && *line;) {
        char *next = strchr(line, '\n');
        if (next)
            *next++ = 0;
        char *q1 = strchr(line, '"'), *q2 = q1 ? strrchr(line, '"') : NULL;
        if (q1 && q2 > q1) {
            char *head[3], *tail[5], comment[160];
            *q1 = 0;
            *q2 = 0;
            snprintf(comment, sizeof comment, "%s", q1 + 1);
            for (char *c = comment + strlen(comment); c > comment && c[-1] == ' ';)
                *--c = 0;
            for (char *c = comment; *c; c++)
                if (*c == '\t')
                    *c = ' ';
            const int32_t nh = split_fields(line, head, 3);         /* rank, user, file, "" */
            char *rest = q2 + 1;
            while (*rest == ',' || *rest == ' ')
                rest++;
            const int32_t nt = split_fields(rest, tail, 5);         /* date, scene, rate, raters, 0 */
            if (nh >= 3 && nt >= 4) {
                const char *d = tail[0];                            /* 2026-03-06-16-46-23 */
                char date[16] = "";
                if (strlen(d) >= 10)
                    snprintf(date, sizeof date, "%.2s.%.2s.%.2s", d + 2, d + 5, d + 8);
                for (char *c = head[2]; *c; c++)
                    if (*c == ',')
                        *c = 0;
                put_text(&p, &left, "%d\t%s\t%s\t%s\t%s\t%d\t%s\t%d\n", atoi(head[0]), head[1], head[2], comment, date,
                         atoi(tail[1]), tail[2], atoi(tail[3]));
            }
        }
        line = next;
    }
    Https_Free(&r);
    return 1;
}

int32_t UnitedRecords_GaloreDownload(const char *user, const char *file, unsigned char **data, uint32_t *size)
{
    char url[640], u[160], f[300];
    HttpsReply r;
    *data = NULL;
    *size = 0;
    Https_Escape(user, u, sizeof u);
    Https_Escape(file, f, sizeof f);
    snprintf(url, sizeof url, "%s/highlights/%s/%s", PROJECT_RECORDS_API, u, f);
    if (!get(url, &r))
        return 0;
    *data = (unsigned char *)malloc(r.length ? r.length : 1);
    if (*data) {
        memcpy(*data, r.body, r.length);
        *size = r.length;
    }
    Https_Free(&r);
    return *data != NULL;
}

int32_t UnitedRecords_SpeedrunReplay(const char *file, unsigned char **data, uint32_t *size)
{
    char url[640], name[200], f[300];
    size_t n = 0;
    HttpsReply r;
    *data = NULL;
    *size = 0;
    /* SWOS 2020's downloadReplayFileEx: the name in lower case, ".rpl" for ".7z" */
    for (; file[n] && n + 4 < sizeof name; n++)
        name[n] = (char)(file[n] >= 'A' && file[n] <= 'Z' ? file[n] + 32 : file[n]);
    name[n] = 0;
    if (n >= 4 && !strcmp(name + n - 4, ".rpl"))
        name[n - 4] = 0;
    else if (!n)
        return 0;
    strcat(name, ".7z");
    Https_Escape(name, f, sizeof f);
    snprintf(url, sizeof url, "%s/replays/%s", PROJECT_RECORDS_API, f);
    if (!get(url, &r))
        return 0;
    *data = (unsigned char *)malloc(r.length ? r.length : 1);
    if (*data) {
        memcpy(*data, r.body, r.length);
        *size = r.length;
    }
    Https_Free(&r);
    return *data != NULL;
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
