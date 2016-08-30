<?php

/**
 * Encode files by directory
 * @author: xusong.lie
 */

$nfiles = 0;
$finish = 0;

function calculate_directory_schedule($dir)
{
    global $nfiles;

    $dir = rtrim($dir, '/');

    $handle = opendir($dir);
    if (!$handle) {
        return false;
    }

    while (($file = readdir($handle))) {
        if ($file == '.' || $file == '..') {
            continue;
        }

        $path = $dir . '/' . $file;

        if (is_dir($path)) {
            calculate_directory_schedule($path);

        } else {
            $infos = explode('.', $file);

            if (strtolower($infos[count($infos)-1]) == 'php') {
                $nfiles++;
            }
        }
    }

    closedir($handle);
}

function encrypt_directory($dir, $new_dir, $expire)
{
    global $nfiles, $finish;

    $dir = rtrim($dir, '/');
    $new_dir = rtrim($new_dir, '/');

    $handle = opendir($dir);
    if (!$handle) {
        return false;
    }

    while (($file = readdir($handle))) {
        if ($file == '.' || $file == '..') {
            continue;
        }

        $path = $dir . '/' . $file;
        $new_path =  $new_dir . '/' . $file;

        if (is_dir($path)) {
            if (!is_dir($new_path)) {
                mkdir($new_path, 0777);
            }

            encrypt_directory($path, $new_path, $expire);

        } else {
            $infos = explode('.', $file);

            if (strtolower($infos[count($infos)-1]) == 'php') {
                if (!empty($expire)) {
                    $result = beast_encode_file($path, $new_path, $expire);
                } else {
                    $result = beast_encode_file($path, $new_path);
                }

                if (!$result) {
                    echo "Failed to encode file `{$path}'\n";
                }

                $finish++;

                $percent = intval($finish / $nfiles * 100);

                printf("\rProcessed encrypt files [%d%%] - 100%%", $percent);

            } else {
                copy($path, $new_path);
            }
        }
    }

    closedir($handle);
}

//////////////////////////////// run here ////////////////////////////////////

$conf = parse_ini_file(dirname(__FILE__) . '/configure.ini');
if (!$conf) {
    exit("Fatal: failed to read configure.ini file\n");
}

$src_path = trim($conf['src_path']);
$dst_path = trim($conf['dst_path']);
$expire   = trim($conf['expire']);

if (empty($src_path) || !is_dir($src_path)) {
    exit("Fatal: source path `{$src_path}' not exists\n\n");
}

if (empty($dst_path) || (!is_dir($dst_path) && !mkdir($dst_path, 0777))) {
    exit("Fatal: can not create directory `{$dst_path}'\n\n");
}

printf("Source code path: %s\n", $src_path);
printf("Destination code path: %s\n", $dst_path);
printf("Expire time: %s\n", $expire);
printf("------------- start process -------------\n");

if ($expire) {
    $expiretime = strtotime($expire);

    $oldtimezone = ini_get('date.timezone');

    ini_set('date.timezone', 'UTC');

    $fixtime = strtotime($expire) - $expiretime;

    $expiretime -= $fixtime;

    ini_set('date.timezone', $oldtimezone);

    $expire = date('Y-m-d H:i:s', $expiretime);
}

$time = microtime(TRUE);

calculate_directory_schedule($src_path);
encrypt_directory($src_path, $dst_path, $expire);

$used = microtime(TRUE) - $time;

printf("\nFinish processed encrypt files, used %f seconds\n", $used);
