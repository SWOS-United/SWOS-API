/*
 * united_api.h -- SWOS United's online API, the game's side (client/src/united_api.c, in
 * SWOS-API): the two sites' addresses, their JSON and XML answers, and the log-in and its
 * session. The game's screens call it; they live in SWOS United (its src/).
 */
#ifndef UNITED_API_H
#define UNITED_API_H

#include "swos_types.h"

/* The site's API: "https://sensiblesoccer.de/index.php?option=com_swosapi&view=<view>", and
 * "&<query>" after it when given. In a static buffer: copy it before the next call. */
const char *United_ApiUrl(const char *view, const char *query);
void        United_ApiUrlTo(char *url, uint32_t cap, const char *view, const char *query);  /* (any thread) */

/* The API's replies are flat XML. The text of the first <tag>...</tag> in [p, end) into out
 * (entities &amp; &lt; &gt; &quot; &apos; decoded); the text just past it, or NULL with out
 * "" when there is none. */
const char *United_XmlText(const char *p, const char *end, const char *tag, char *out, uint32_t cap);
/* The next <tag>...</tag> block in [p, end): *blockEnd set past it; NULL when none. */
const char *United_XmlBlock(const char *p, const char *end, const char *tag, const char **blockEnd);

/* Which site the log-in, the rankings and the leagues use: 0 sensiblesoccer.de (its XML API
 * above), 1 swos.swosunited.com (PROJECT_ACCOUNT_API, JSON). Set from [netplay] account_site
 * ("sensiblesoccer" | "swosunited"); SWOS_UNITED_SITE in the environment wins. */
void        United_SetSite(const char *name);
int32_t     United_NewSite(void);

/* The new site's API: PROJECT_ACCOUNT_API "<path>", and "?<query>" after it when given. In a
 * static buffer, like United_ApiUrl. */
const char *United_NewApiUrl(const char *path, const char *query);
void        United_NewApiUrlTo(char *url, uint32_t cap, const char *path, const char *query);

/* Its replies are JSON. A span of it: an object, an array or one value. */
typedef struct { const char *p; int32_t n; } UnitedJson;
/* The value at a JSONPath ("$.users", "$.details.name") in a span; 0 when there is none. */
int32_t     United_JsonFind(UnitedJson in, const char *path, UnitedJson *out);
/* The next member of an array or object, in one pass (*pos starts at 0): its value, and for an
 * object its key (the quotes left out; key may be NULL). 0 at the end. */
int32_t     United_JsonNext(UnitedJson in, int32_t *pos, UnitedJson *key, UnitedJson *val);
/* The value at path as text: a string decoded, a number or true/false as written, null or
 * nothing "". The site sends a number as a number or as text depending on the field. */
void        United_JsonText(UnitedJson in, const char *path, char *out, uint32_t cap);
/* ... and as a number (a number, or text holding one); 0 when it is neither. */
int32_t     United_JsonInt(UnitedJson in, const char *path);
/* "status":"ok" -- what every good reply says. */
int32_t     United_JsonOk(const char *body, uint32_t len);



/* The log-in: Begin with the name and password (on the site United_NewSite chooses), then Poll
 * each frame -- 0 still asking, 1 logged in, -1 refused (United_LoginError says why). */
void        United_LoginBegin(const char *user, const char *password);
int32_t     United_LoginPoll(void);
const char *United_LoginError(void);
void        United_LoginRefuse(const char *why);   /* (a log-in the caller will not have) */
/* The logged-in player (0 / "": nobody). */
int32_t     United_UserId(void);
const char *United_UserName(void);
const char *United_Password(void);      /* (kept in memory for this player's own results only) */
const char *United_ApiKey(void);        /* the new site's key for this session; "" none (old site) */


#endif /* UNITED_API_H */
