<?php

/**
 * Usage:
 * php encode_file.php --oldfile old_file_path --newfile new_file_path --encrypt DES --expire "2016-10-10 10:10:10"
 * @author: liexusong
 */

$newfile  = null;
$oldfile  = null;
$datetime = 0;
$encrypt  = 'DES';

for ($i = 1; $i < count($argv); $i++)
{
    switch ($argv[$i]) {
    case '--newfile':
        $newfile = @$argv[++$i];
        break;
    case '--oldfile':
        $oldfile = @$argv[++$i];
        break;
    case '--expire':
        $datetime = @$argv[++$i];
        break;
    case '--encrypt':
        $encrypt = @$argv[++$i];
        break;
    default:
        echo "Fatal error: Invaild option `{$argv[$i]}'\n";
        exit(1);
    }
}

switch ($encrypt) {
case 'DES':
    $type = BEAST_ENCRYPT_TYPE_DES;
    break;
case 'AES':
    $type = BEAST_ENCRYPT_TYPE_AES;
    break;
case 'BASE64':
    $type = BEAST_ENCRYPT_TYPE_BASE64;
    break;
default:
    $type = BEAST_ENCRYPT_TYPE_DES;
    break;
}

if (empty($oldfile) || !file_exists($oldfile)) {
    echo "Encrypt file `{$oldfile}' not found!\n";
    exit(1);
}

if (empty($newfile)) {

    $paths = explode('.', basename($oldfile));

    $exten = $paths[count($paths)-1];

    unset($paths[count($paths)-1]);

    $name = implode('.', $paths);

    $newfile = dirname($oldfile) . '/' . $name . '_enc.' . $exten;
}

echo "Starting encrypt `{$oldfile}' and save to `{$newfile}'\n";
echo "Expire time: {$datetime}, using encrypt type: $encrypt\n";

$expire = $datetime ? strtotime($datetime) : 0;

if (beast_encode_file($oldfile, $newfile, $expire, $type)) {
    echo "Encrypt file success!\n";
} else {
    echo "Encrypt file failure!\n";
}
