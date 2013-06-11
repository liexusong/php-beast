<?php

function getext($file) {
	$list = explode('.', $file);
	return strtolower($list[count($list)-1]);
}

function encrypt_project($project, $new_project) {
	$project = rtrim($project, '/');
	$new_project = rtrim($new_project, '/');
	
	if (!file_exists($new_project)) {
		if (!mkdir($new_project)) {
			printf("[failed] failed to call `mkdir()' function\n");
			return false;
		}
	}
	
	$hdl_o = opendir($project);
	$hdl_n = opendir($new_project);
	if (!$hdl_o || !$hdl_n) {
		if ($hdl_o) closedir($hdl_o);
		if ($hdl_n) closedir($hdl_n);
		printf("[failed] failed to call `opendir()' function\n");
		return false;
	}
	
	while (($file = readdir($hdl_o)) !== false) {
		if ($file == '.' || $file == '..') {
			continue;
		}
		$path = $project.'/'.$file;
		if (is_dir($path)) {
			encrypt_project($path, $new_project.'/'.$file);
		} elseif (is_file($path) && getext($file) == 'php') {
			beast_encode_file($path, $new_project.'/'.$file);
		} else {
			copy($path, $new_project.'/'.$file);
		}
	}
	
	closedir($hdl_o);
	closedir($hdl_n);
	return true;
}

$stdin = fopen("php://stdin", "r");
$stdout = fopen("php://stdout", "w");
if (!$stdin || !$stdout) {
	if ($stdin) fclose($stdin);
	if ($stdout) fclose($stdout);
	exit("[failed] failed to open I/O stream\n");
}

fwrite($stdout, "Please enter project path: ");
$project = fgets($stdin);
$project = trim($project);

fwrite($stdout, "Please enter output project path: ");
$new_project = fgets($stdin);
$new_project = trim($new_project);

$start = time();
fwrite($stdout, "Encrypting...\n");

encrypt_project($project, $new_project); /* encrypt project */

$spend = time() - $start;
fwrite($stdout, "Finish encrypt, spend {$spend} seconds.\n");

?>