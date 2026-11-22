<?php
/*
 * A replacement for swosapi_v3.php's delete_replay_file() (sensiblesoccer.de/swos2020/swosapi_v3.php,
 * ?option=delete_replay_file&filename=...): paste it over the old function.
 *
 * The old one unlinked "./replays/" . $filename for anyone who asked -- any player's replay, and,
 * with "../" in the name, any file the web server could write. This one deletes a replay only
 * when it is nobody's any more:
 *   - the name is a run's .srp ("<user>-YYYY-MMDD-HHMMSS-auto.srp"), no folder in it;
 *   - no record names it (speedrun_records' best_replay_filename / current_replay_filename) --
 *     SWOS United asks after a sync has replaced it, so it is free then; a file still on a board is
 *     never deleted, whoever asks.
 * SWOS 2020's .7z are left alone, as its own delete left them (it named the .rpl, never there).
 * Answers "OK" or "Failed", as before.
 */
function delete_replay_file()
{
    global $filename;

    $name = (string)$filename;
    if (!preg_match('/\A[A-Za-z0-9_^][A-Za-z0-9_.^-]{0,39}-\d{4}-\d{4}-\d{6}-auto\.srp\z/', $name)) {
        echo "Failed\n";
        return;
    }
    $conn = set_connection();
    if (!$conn) {
        echo "Failed\n";
        return;
    }
    $stmt = mysqli_prepare($conn,
        'SELECT COUNT(*) FROM speedrun_records WHERE best_replay_filename = ? OR current_replay_filename = ?');
    if (!$stmt) {
        echo "Failed\n";
        return;
    }
    mysqli_stmt_bind_param($stmt, 'ss', $name, $name);
    mysqli_stmt_execute($stmt);
    mysqli_stmt_bind_result($stmt, $uses);
    mysqli_stmt_fetch($stmt);
    mysqli_stmt_close($stmt);
    if ((int)$uses > 0) {               /* (still on a board: kept) */
        echo "Failed\n";
        return;
    }
    $path = __DIR__ . '/replays/' . $name;
    if (is_file($path) && unlink($path))
        echo "OK\n";
    else
        echo "Failed\n";
}
