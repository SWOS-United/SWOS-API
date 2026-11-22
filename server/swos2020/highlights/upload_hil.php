<?php
/*
 * swos2020/highlights/upload_hil.php?username=<player> -- a SWOS GALORE highlight, uploaded when it
 * is shared (SHARE HIGHLIGHTS). Stored as highlights/<player>/<file>, where the boards download it.
 *
 * SWOS 2020's upload_hil.php took the folder from ?username= as it came -- "../" and all -- and
 * stored any file, any name, over any other. This one stores a file only when:
 *   - it is a POST with one file in "myFile", uploaded whole, at most MAX_BYTES;
 *   - the player's name is a name (letters, digits, space _ . ^ -; no "..", no leading dot);
 *   - the file's name is a highlight's ("<name>.HIL", the same characters and ( ));
 *   - its first bytes are a highlights file's: SWOS 2020's / the PC's (08 00 00 00), SWOS's own
 *     (00 00 00 0C) or SWOS United's (JSON, "{");
 *   - no file of that name is in the player's folder yet (nothing is replaced: 409, which the game
 *     takes as "there already");
 *   - the address has sent fewer than RATE_MAX in the last RATE_WINDOW seconds.
 * The answer: 200 "OK", else 4xx/5xx and why.
 *
 * Deploy it with this folder's .htaccess, which keeps every other file here from running.
 */
declare(strict_types=1);

const MAX_BYTES   = 4 * 1024 * 1024;   /* SWOS 2020's ~600 KB, SWOS United's JSON ~1.2 MB for 10 scenes */
const RATE_MAX    = 30;
const RATE_WINDOW = 3600;
const USER_RE     = '/\A[A-Za-z0-9_^-][A-Za-z0-9 _.^-]{0,39}\z/';
const FILE_RE     = '/\A[A-Za-z0-9_^(-][A-Za-z0-9 _.^()-]{0,90}\.[Hh][Ii][Ll]\z/';

header('Content-Type: text/plain; charset=utf-8');
header('X-Content-Type-Options: nosniff');
header('Cache-Control: no-store');

function fail(int $status, string $why): void
{
    http_response_code($status);
    echo $why, "\n";
    exit;
}

function rate_count(): int
{
    $dir = rtrim(sys_get_temp_dir(), '/\\') . '/swos_hil_rate';
    if (!is_dir($dir) && !@mkdir($dir, 0700) && !is_dir($dir))
        return 0;
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

$user = (string)($_GET['username'] ?? '');
if (!preg_match(USER_RE, $user) || strpos($user, '..') !== false)
    fail(400, 'not a player name');

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
if (!preg_match(FILE_RE, $name) || strpos($name, '..') !== false)
    fail(400, 'not a highlight name');

$head = (string)file_get_contents($f['tmp_name'], false, null, 0, 4);
if ($head !== "\x08\x00\x00\x00" && $head !== "\x00\x00\x00\x0C" && ($head === '' || $head[0] !== '{'))
    fail(415, 'not a highlights file');

if (rate_count() > RATE_MAX)
    fail(429, 'too many uploads, try again later');

$dir = __DIR__ . '/' . $user;
if (!is_dir($dir) && !@mkdir($dir, 0755) && !is_dir($dir))
    fail(500, 'could not store it');

$dest = $dir . '/' . $name;
$out = @fopen($dest, 'xb');
if (!$out)
    fail(409, 'that highlight is there already');
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
