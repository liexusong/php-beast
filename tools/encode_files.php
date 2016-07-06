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

function encrypt_directory($dir, $new_dir)
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

            encrypt_directory($path, $new_path);

        } else {
            $infos = explode('.', $file);

            if (strtolower($infos[count($infos)-1]) == 'php') {
                if (!beast_encode_file($path, $new_path)) {
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


if (count($argv) < 3) {
    exit("Usage: php encode_files.php <source path> <destination path>\n\n");
}

$src_path = $argv[1];
$dst_path = $argv[2];

if (!is_dir($src_path)) {
    exit("Fatal: source path `{$src_path}' not exists\n\n");
}

if (!is_dir($dst_path) && !mkdir($dst_path, 0777)) {
    exit("Fatal: can not create directory `{$dst_path}'\n\n");
}

$time = microtime(TRUE);

calculate_directory_schedule($src_path);
encrypt_directory($src_path, $dst_path);

$used = microtime(TRUE) - $time;

printf("\nFinish processed encrypt files, used %f seconds\n", $used);
