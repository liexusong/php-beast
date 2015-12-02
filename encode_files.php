<?php

/**
 * Encode files by directory
 * @author: Howard.Lee
 */

function encode_files($dir, $new_dir)
{
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
            encode_files($path, $new_path);
        } else {
            $infos = explode('.', $file);

            if (strtolower($infos[count($infos)-1]) == 'php') {
                if (!beast_encode_file($path, $new_path)) {
                    echo "Failed to encode file `{$path}'\n";
                }
            } else {
                copy($path, $new_path);
            }
        }
    }
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

encode_files($old_path, $new_path);

