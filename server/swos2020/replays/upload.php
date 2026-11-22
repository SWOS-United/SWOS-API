<?php
/*
 * swos2020/replays/upload.php -- a speedrun's replay, uploaded with its record.
 *
 * SWOS United posts the run's .SRP here, SWOS 2020 its .7z (of the run's .RPL), both as
 * `curl --form myFile=@<file>` sends it: one file field, "myFile". The record (swosapi_v3.php's
 * sync_row_*) then names the file; the boards download it from this folder.
 *
 * SWOS 2020's upload.php saved any file, under any name, from anyone -- over another player's
 * replay, or as a script in a web folder. This one stores a file only when:
 *   - it is a POST with one file in "myFile", uploaded whole, at most MAX_BYTES;
 *   - its name is a run's: "<user>-YYYY-MMDD-HHMMSS-auto.srp" (SWOS United) or ".7z" (SWOS 2020);
 *   - its first bytes are that format's ("SWRP" an .SRP, 7z's signature a .7z);
 *   - no file of that name is there yet (a run's name has its second in it: nothing is replaced);
 *   - the address has sent fewer than RATE_MAX in the last RATE_WINDOW seconds.
 * The answer: 200 "OK", else 4xx/5xx and why (SWOS United reads the status; SWOS 2020 ignores it).
 *
 * Deploy it with this folder's .htaccess, which keeps every other file here from running.
 */
declare(strict_types=1);

const MAX_BYTES   = 4 * 1024 * 1024;   /* a speedrun's .SRP is ~50 KB, a whole match's ~1 MB */
const RATE_MAX    = 30;                /* uploads an address may make ... */
const RATE_WINDOW = 3600;              /* ... in this many seconds */
const NAME_RE     = '/\A[A-Za-z0-9_^][A-Za-z0-9_.^-]{0,39}-\d{4}-\d{4}-\d{6}-auto\.(srp|7z)\z/';

header('Content-Type: text/plain; charset=utf-8');
header('X-Content-Type-Options: nosniff');
header('Cache-Control: no-store');

function fail(int $status, string $why): void
{
    http_response_code($status);
    echo $why, "\n";
    exit;
}

/* How many uploads this address has made in the window, this one counted: a small file per
 * address (a hash of it, never the address itself) in the system's temp folder, locked. */
function rate_count(): int
{
    $dir = rtrim(sys_get_temp_dir(), '/\\') . '/swos_replay_rate';
    if (!is_dir($dir) && !@mkdir($dir, 0700) && !is_dir($dir))
        return 0;                                   /* (no folder: not limited, not refused) */
    $file = $dir . '/' . hash('sha256', (string)($_SERVER['REMOTE_ADDR'] ?? '')) . '.json';
    $fh = @fopen($file, 'c+');
    if (!$fh)
        return 0;
    flock($fh, LOCK_EX);
    $now = time();
    $times = json_decode((string)stream_get_contents($fh), true);
    $times = array_values(array_filter(is_array($times) ? $times : [],
                                       function ($t) use ($now) { return is_int($t) && $t > $now - RATE_WINDOW; }));
    $times[] = $now;
    ftruncate($fh, 0);
    rewind($fh);
    fwrite($fh, (string)json_encode($times));
    flock($fh, LOCK_UN);
    fclose($fh);
    return count($times);
}

if (($_SERVER['REQUEST_METHOD'] ?? '') !== 'POST')
    fail(405, 'POST only');

$f = $_FILES['myFile'] ?? null;
if (!is_array($f) || !isset($f['error']) || is_array($f['error']))
    fail(400, 'no file in myFile');
if ($f['error'] === UPLOAD_ERR_INI_SIZE || $f['error'] === UPLOAD_ERR_FORM_SIZE)
    fail(413, 'too large');
if ($f['error'] !== UPLOAD_ERR_OK || !is_uploaded_file($f['tmp_name']))
    fail(400, 'the upload did not arrive whole');

$size = filesize($f['tmp_name']);
if ($size === false || $size <= 0 || $size > MAX_BYTES)
    fail(413, 'too large');

$name = (string)$f['name'];
if (!preg_match(NAME_RE, $name, $m))
    fail(400, 'not a replay name');
$magic = $m[1] === 'srp' ? 'SWRP' : "7z\xBC\xAF\x27\x1C";
$head = file_get_contents($f['tmp_name'], false, null, 0, strlen($magic));
if ($head !== $magic)
    fail(415, 'not a replay');

if (rate_count() > RATE_MAX)
    fail(429, 'too many uploads, try again later');

/* Created only if it is not there ('x'): two uploads of one name cannot both win, and nothing
 * already here is ever written over. */
$dest = __DIR__ . '/' . $name;
$out = @fopen($dest, 'xb');
if (!$out)
    fail(409, 'that replay is there already');
$in = fopen($f['tmp_name'], 'rb');
$copied = $in ? stream_copy_to_stream($in, $out) : false;
if ($in)
    fclose($in);
fclose($out);
if ($copied !== $size) {
    @unlink($dest);
    fail(500, 'could not store it');
}
@chmod($dest, 0644);
echo "OK\n";
