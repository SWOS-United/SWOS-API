# SWOS-API client

The game's side of SWOS United's online API, in C. SWOS United builds it from this submodule
(its `fork.mk` and `fork.cmake`); its screens and mods call it and never talk to the network
themselves.

| file | what |
|---|---|
| `include/https_client.h`, `src/https_client.c` | HTTPS (mbedTLS, SWOS United's `third_party/mbedtls`), the server's certificate checked against `cacert.pem` beside the game; blocking, or one request on a thread of its own |
| `include/united_api.h`, `src/united_api.c` | the two sites' addresses (sensiblesoccer.de's XML API; swos.swosunited.com's JSON API, `[netplay] account_site`), their answers' JSON / XML helpers, the log-in (`United_LoginBegin` / `United_LoginPoll`) and its session (user, account id, `api_key`) |
| `include/united_records.h`, `src/united_records.c` | the challenges' and speedruns' records: SWOS 2020's `galoreapi_v15.php` and `swosapi_v3.php` (blocking -- from a worker thread) |

Built in SDL builds only (`SWOS_SDL_BUILD`); what needs the network also needs `SWOS_HTTPS`. It
includes the game's headers (`swos_types.h`, `project_identity.h`, `mjson.h`), so it builds with
the game, not on its own.

New API client code goes here, never in SWOS United's `src/`.
