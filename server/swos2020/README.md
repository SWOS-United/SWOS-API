# server/swos2020 -- hardening for SWOS 2020's speedrun folder on sensiblesoccer.de

SWOS United keeps its speedrun records where SWOS 2020 kept them (`sensiblesoccer.de/swos2020/`,
the game's `PROJECT_RECORDS_API`) and uploads each run's replay there as an `.SRP`. These files
replace the open parts of SWOS 2020's PHP there. Deploy them by hand -- nothing here is deployed
by the game or by a script.

| file | goes to | what it fixes |
|---|---|---|
| `replays/upload.php` | `swos2020/replays/upload.php` | the old one stored any file, any name, from anyone, over any other |
| `replays/.htaccess` | `swos2020/replays/.htaccess` | nothing in `replays/` runs or is served as a script, except `upload.php` |
| `delete_replay_file.php` | paste over `delete_replay_file()` in `swos2020/swosapi_v3.php` | the old one deleted any file the web server could write, for anyone |
| `highlights/upload_hil.php` | `swos2020/highlights/upload_hil.php` | SWOS GALORE's SHARE: the old one took its folder from `?username=` as it came (`../` and all) and stored any file over any other |
| `highlights/.htaccess` | `swos2020/highlights/.htaccess` | nothing in `highlights/` (or a player's folder in it) runs or is served as a script, except `upload_hil.php` |

SWOS 2020's game keeps working with all of them: it uploads `<name>.7z` and its highlights the same way, and its
delete never found anything to delete.

After deploying:
1. Look through `swos2020/replays/` for anything that is not `.7z`, `.srp`, `.rpl` or `.idx`
   (and `upload.php`, `.htaccess`), and through `swos2020/highlights/` (its players' folders) for
   anything that is not `.HIL`: the old uploads took any file, so check nothing else got in.
2. Check php.ini allows the upload (`upload_max_filesize`, `post_max_size` >= 5M), and that
   Apache reads the `.htaccess` (`AllowOverride` for the folder).
3. Finish a speedrun in SWOS United, logged in: `SWOS.log` says
   `[online-records] speedrun sent (... replay <name>.srp ...)`, and the run plays from LATEST RUNS.

Still open (not fixed here):
- `galoreapi_v15.php`'s `upsert_hil_data` makes the folder `highlights/<username>` from the request
  as it comes (`mkdir('highlights/' . $username)`): check the name there as `upload_hil.php` does.
  (Its queries are prepared statements already.)
- `swosapi_v3.php` puts its request values straight into its SQL; every query needs prepared
  statements (`mysqli_prepare`, as `delete_replay_file.php` does).
- Nothing checks WHO sends a record or a replay: the API takes a user name and no password. The
  fix is the account log-in SWOS United already has (`swos.swosunited.com/api`, an `api_key`),
  checked by the records API.
