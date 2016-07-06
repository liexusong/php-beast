<?php

/**
 * Encode files by directory
 * @author: xusong.lie
 */

$files = 0;
$finish = 0;

function calculate_directory_schedule($dir)
{
    global $files;

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
            calculate_directory_schedule($path);

        } else {
            $infos = explode('.', $file);

            if (strtolower($infos[count($infos)-1]) == 'php') {
                $files++;
            }
        }
    }

    closedir($handle);
}

function encrypt_directory($dir, $new_dir)
{
    global $files, $finish;

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

                printf("\rProcessed files [%d%%] - 100%%", intval($finish / $files));

            } else {
                copy($path, $new_path);
            }
        }
    }

    closedir($handle);
}


if (count($argv) < 3) {
    exit("Usage: encode_files.php <old_path> <new_path>\n\n");
}

$old_path = $argv[1];
$new_path = $argv[2];

if (!is_dir($old_path)) {
    exit("Fatal: path `{$old_path}' not exists\n\n");
}

if (!is_dir($new_path) && !mkdir($new_path, 0777)) {
    exit("Fatal: can not create directory `{$newpath}'\n\n");
}

$time = time();

calculate_directory_schedule($old_path);
encrypt_directory($old_path, $new_path);

$used = time() - $time;

printf("\nFinish processed files, used %d seconds\n", $used);

