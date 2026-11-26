/*
 * united_live2020.c -- SWOS 2020's live matches (include/united_live2020.h): the list over HTTPS
 * (https_client.c), a match's stream as a WebSocket client of its own (RFC 6455, the client's
 * side only: masked frames out, the server's unmasked frames in, ping answered, fragments joined).
 */
#if defined(SWOS_SDL_BUILD) && defined(SWOS_HTTPS)

#include "united_live2020.h"
#include "https_client.h"
#include "project_identity.h"
#include "mjson.h"

#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#include <netinet/in.h>
#include <SDL.h>

#ifndef PROJECT_LIVE2020_LIST_URL
#define PROJECT_LIVE2020_LIST_URL   "https://swos.eu:2020/api/live"
#define PROJECT_LIVE2020_HOST       "swos.eu"
#define PROJECT_LIVE2020_WATCH_PORT 2019
#endif

enum { kMaxMessage = 1 << 20 };

/* ---- the list ---- */

static void upper(char *s)          /* (the menus' font has capitals only; no spaces around) */
{
    char *o = s, *c = s;
    while (*c == ' ')
        c++;
    for (; *c; c++)
        *o++ = (char)(*c >= 'a' && *c <= 'z' ? *c - 32 : *c);
    while (o > s && o[-1] == ' ')
        o--;
    *o = 0;
}

int32_t United_Live2020List(UnitedLive2020 *out, int32_t cap)
{
    HttpsReply r;
    int32_t n = 0;
    memset(&r, 0, sizeof r);
    Https_Request(PROJECT_LIVE2020_LIST_URL, NULL, &r);
    if (r.status != 200 || !r.body) {
        fprintf(stderr, "[live2020] %s: %d\n", PROJECT_LIVE2020_LIST_URL, (int)r.status);
        Https_Free(&r);
        return -1;
    }
    for (int32_t i = 0; i < 64; i++) {
        char path[64];
        const char *p;
        int plen;
        UnitedLive2020 m;
        double v;
        snprintf(path, sizeof path, "$[%d]", (int)i);
        if (mjson_find(r.body, (int)r.length, path, &p, &plen) != MJSON_TOK_OBJECT)
            break;
        memset(&m, 0, sizeof m);
        if (mjson_get_string(p, plen, "$.streamId", m.streamId, sizeof m.streamId) <= 0)
            continue;
        {   /* (the streamer is team 2's when streamerIsTeam1 is false; absent: team 1's, as swos.eu) */
            int b = 1;
            const int t2 = mjson_get_bool(p, plen, "$.streamerIsTeam1", &b) && !b;
            mjson_get_string(p, plen, "$.username", m.players[t2], sizeof m.players[0]);
            mjson_get_string(p, plen, "$.opponent", m.players[!t2], sizeof m.players[0]);
        }
        mjson_get_string(p, plen, "$.team1Name", m.teams[0], sizeof m.teams[0]);
        mjson_get_string(p, plen, "$.team2Name", m.teams[1], sizeof m.teams[1]);
        for (int k = 0; k < 2; k++) {
            upper(m.players[k]);
            upper(m.teams[k]);
        }
        if (mjson_get_number(p, plen, "$.team1Goals", &v)) m.goals[0] = (int32_t)v;
        if (mjson_get_number(p, plen, "$.team2Goals", &v)) m.goals[1] = (int32_t)v;
        if (mjson_get_number(p, plen, "$.gameTime", &v)) m.minute = (int32_t)v;
        m.state = mjson_get_number(p, plen, "$.matchState", &v) ? (int32_t)v : 1;
        if (out && n < cap)
            out[n] = m;
        n++;
    }
    Https_Free(&r);
    return out && n > cap ? cap : n;
}

/* ---- a match's stream ---- */

struct UnitedLive2020Watch {
    int       sock;
    uint8_t  *buf;              /* the message being read (a frame's fragments joined) */
    uint32_t  cap, len;
    uint8_t  *in;               /* what came off the socket, not yet taken */
    uint32_t  inCap, inLen, inAt;
};

static int send_all(int s, const uint8_t *p, size_t n)
{
    while (n) {
        const ssize_t k = send(s, p, n, 0);
        if (k <= 0)
            return -1;
        p += k;
        n -= (size_t)k;
    }
    return 0;
}

/* n bytes off the socket (through its buffer). 0 ok. */
static int read_bytes(UnitedLive2020Watch *w, uint8_t *to, uint32_t n)
{
    while (n) {
        if (w->inAt == w->inLen) {
            ssize_t k;
            if (!w->in && !(w->in = (uint8_t *)malloc(w->inCap = 65536)))
                return -1;
            k = recv(w->sock, w->in, w->inCap, 0);
            if (k <= 0)
                return -1;
            w->inLen = (uint32_t)k;
            w->inAt = 0;
        }
        const uint32_t take = w->inLen - w->inAt < n ? w->inLen - w->inAt : n;
        memcpy(to, w->in + w->inAt, take);
        w->inAt += take;
        to += take;
        n -= take;
    }
    return 0;
}

/* A frame from this side: masked, as a client's must be (RFC 6455 5.3). */
static int send_frame(UnitedLive2020Watch *w, uint8_t opcode, const uint8_t *p, uint32_t n)
{
    uint8_t head[14], mask[4];
    uint32_t h = 0;
    uint8_t *out;
    int rc;
    const uint32_t r = (uint32_t)SDL_GetPerformanceCounter() ^ 0x5A17C3E9u;
    head[h++] = (uint8_t)(0x80 | opcode);
    if (n < 126)
        head[h++] = (uint8_t)(0x80 | n);
    else {
        head[h++] = 0x80 | 126;
        head[h++] = (uint8_t)(n >> 8);
        head[h++] = (uint8_t)n;
    }
    for (int i = 0; i < 4; i++)
        head[h++] = mask[i] = (uint8_t)(r >> (8 * i));
    out = (uint8_t *)malloc(h + n);
    if (!out)
        return -1;
    memcpy(out, head, h);
    for (uint32_t i = 0; i < n; i++)
        out[h + i] = p[i] ^ mask[i & 3];
    rc = send_all(w->sock, out, h + n);
    free(out);
    return rc;
}

static int open_socket(const char *host, int port)
{
    struct addrinfo hints, *res = NULL;
    char ps[8];
    int s;
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    snprintf(ps, sizeof ps, "%d", port);
    if (getaddrinfo(host, ps, &hints, &res) != 0 || !res) {
        fprintf(stderr, "[live2020] %s: cannot resolve it\n", host);
        return -1;
    }
    s = socket(AF_INET, SOCK_STREAM, 0);
    if (s >= 0) {
        const int fl = fcntl(s, F_GETFL, 0);
        struct pollfd pf;
        int err = 0;
        socklen_t el = sizeof err;
        fcntl(s, F_SETFL, fl | O_NONBLOCK);
        if (connect(s, res->ai_addr, res->ai_addrlen) != 0 && errno != EINPROGRESS)
            err = errno;
        pf.fd = s;
        pf.events = POLLOUT;
        if (!err && (poll(&pf, 1, 4000) != 1 || getsockopt(s, SOL_SOCKET, SO_ERROR, &err, &el) != 0))
            err = ETIMEDOUT;
        fcntl(s, F_SETFL, fl);
        if (err) {
            fprintf(stderr, "[live2020] %s:%d: %s\n", host, port, strerror(err));
            close(s);
            s = -1;
        }
    }
    freeaddrinfo(res);
    return s;
}

UnitedLive2020Watch *United_Live2020Open(const char *streamId)
{
    UnitedLive2020Watch *w;
    char req[512], line[1024];
    uint32_t got = 0;
    int s;
    if (!streamId || !streamId[0] || strpbrk(streamId, " /?#\r\n"))
        return NULL;
    s = open_socket(PROJECT_LIVE2020_HOST, PROJECT_LIVE2020_WATCH_PORT);
    if (s < 0)
        return NULL;
    {   /* (the handshake within seconds; the stream itself then waits as long as the match) */
        struct timeval tv = { 5, 0 };
        setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);
        setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof tv);
    }
    snprintf(req, sizeof req,
             "GET /watch/%s HTTP/1.1\r\nHost: %s:%d\r\nUpgrade: websocket\r\nConnection: Upgrade\r\n"
             "Sec-WebSocket-Key: U1dPU1VuaXRlZExpdmUyMA==\r\nSec-WebSocket-Version: 13\r\n"
             "User-Agent: SWOSUnited/1\r\n\r\n",
             streamId, PROJECT_LIVE2020_HOST, PROJECT_LIVE2020_WATCH_PORT);
    if (send_all(s, (const uint8_t *)req, strlen(req)) != 0) {
        close(s);
        return NULL;
    }
    w = (UnitedLive2020Watch *)calloc(1, sizeof *w);
    if (!w) {
        close(s);
        return NULL;
    }
    w->sock = s;
    /* the answer's head, a byte at a time up to its blank line (what follows is the stream's) */
    while (got + 1 < sizeof line) {
        if (read_bytes(w, (uint8_t *)line + got, 1) != 0)
            break;
        got++;
        if (got >= 4 && !memcmp(line + got - 4, "\r\n\r\n", 4))
            break;
    }
    line[got] = 0;
    if (strncmp(line, "HTTP/1.1 101", 12) != 0) {
        char *eol = strchr(line, '\r');
        if (eol)
            *eol = 0;
        fprintf(stderr, "[live2020] /watch/%s: %s\n", streamId, got ? line : "no answer");
        United_Live2020Close(w);
        return NULL;
    }
    {
        struct timeval tv = { 0, 0 };
        setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);
    }
    return w;
}

int32_t United_Live2020Read(UnitedLive2020Watch *w, int32_t *text, const uint8_t **data, uint32_t *len)
{
    int32_t kind = -1;
    if (!w)
        return -1;
    w->len = 0;
    for (;;) {
        uint8_t h[2], ext[8];
        uint64_t n;
        if (read_bytes(w, h, 2) != 0)
            return -1;
        const int fin = (h[0] & 0x80) != 0, op = h[0] & 0x0F, masked = (h[1] & 0x80) != 0;
        n = h[1] & 0x7F;
        if (n == 126) {
            if (read_bytes(w, ext, 2) != 0)
                return -1;
            n = ((uint64_t)ext[0] << 8) | ext[1];
        } else if (n == 127) {
            if (read_bytes(w, ext, 8) != 0)
                return -1;
            n = 0;
            for (int i = 0; i < 8; i++)
                n = (n << 8) | ext[i];
        }
        uint8_t mask[4] = { 0, 0, 0, 0 };
        if (masked && read_bytes(w, mask, 4) != 0)
            return -1;
        if (n > kMaxMessage || w->len + n > kMaxMessage)
            return -1;
        if (w->len + n + 1 > w->cap) {
            uint8_t *b = (uint8_t *)realloc(w->buf, (size_t)(w->len + n + 1));
            if (!b)
                return -1;
            w->buf = b;
            w->cap = (uint32_t)(w->len + n + 1);
        }
        uint8_t *at = w->buf + w->len;
        if (n && read_bytes(w, at, (uint32_t)n) != 0)
            return -1;
        if (masked)
            for (uint64_t i = 0; i < n; i++)
                at[i] ^= mask[i & 3];
        if (op == 0x8)                                  /* close */
            return -1;
        if (op == 0x9) {                                /* ping: pong, the same bytes */
            if (send_frame(w, 0xA, at, (uint32_t)n) != 0)
                return -1;
            continue;
        }
        if (op == 0xA)                                  /* (a pong) */
            continue;
        if (op == 0x1 || op == 0x2)
            kind = op == 0x1;
        else if (op != 0x0)
            continue;                                   /* (nothing else is the server's) */
        w->len += (uint32_t)n;
        if (fin && kind >= 0) {
            w->buf[w->len] = 0;                         /* (a text, terminated) */
            *text = kind;
            *data = w->buf;
            *len = w->len;
            return 0;
        }
    }
}

void United_Live2020Stop(UnitedLive2020Watch *w)
{
    if (w && w->sock >= 0)
        shutdown(w->sock, SHUT_RDWR);
}

void United_Live2020Close(UnitedLive2020Watch *w)
{
    if (!w)
        return;
    if (w->sock >= 0)
        close(w->sock);
    free(w->buf);
    free(w->in);
    free(w);
}

#endif
