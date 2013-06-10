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

$stdin = fopen("php://stdin");

?>