<?php

$nums = array();

for ($i = 0; $i < 8; $i++) {
	$nums[] = sprintf("0x%02x", rand(0, 255));
}

$key = implode(', ', $nums);

$content = <<<CONTENT
/***************************************
 * This is the key for Beast extension *
 ***************************************/
char __authkey[8] = { $key };
CONTENT;

file_put_contents('./key.c', $content);
