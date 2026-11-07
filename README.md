# SWOS-API

SWOS United's online API: the website's endpoints (swos.swosunited.com/api/*) and the game's
client for them. A submodule of SWOS United (`third_party/swos-api`).

The API is documented in this repository's issues: the log-in (`POST /api/auth/key`, an `api_key`
sent with every later call), user details, rankings, seasons, tournaments and match reporting.

SWOS 2020's old API (its `php_manager`: galoreapi, the speedrun API, the uploads) is not here --
it is SWOS 2020's, in its own repository.

**Secrets are never committed:** database logins, webhooks and keys live in files that
`.gitignore` keeps out, with a `.sample` beside each showing its shape.
