/*
  +----------------------------------------------------------------------+
  | PHP Version 5                                                        |
  +----------------------------------------------------------------------+
  | Copyright (c) 1997-2007 The PHP Group                                |
  +----------------------------------------------------------------------+
  | This source file is subject to version 3.01 of the PHP license,      |
  | that is bundled with this package in the file LICENSE, and is        |
  | available through the world-wide-web at the following url:           |
  | http://www.php.net/license/3_01.txt                                  |
  | If you did not receive a copy of the PHP license and are unable to   |
  | obtain it through the world-wide-web, please send a note to          |
  | license@php.net so we can mail you a copy immediately.               |
  +----------------------------------------------------------------------+
  | Author: Liexusong <280259971@qq.com>                                 |
  +----------------------------------------------------------------------+
*/

/* $Id: header,v 1.16.2.1.2.1 2007/01/01 19:32:09 iliaa Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "zend.h"
#include "zend_API.h"
#include "zend_compile.h"

#include "php.h"
#include "php_ini.h"
#include "ext/standard/info.h"
#include "php_streams.h"
#include "php_beast.h"
#include "encrypt.h"

/* If you declare any globals in php_beast.h uncomment this:
ZEND_DECLARE_MODULE_GLOBALS(beast)
*/

#define HASH_INIT_SIZE          32
#define FREE_CACHE_LENGTH       100
#define DEFAULT_MAX_CACHE_SIZE  1048576

struct beast_cache_item {
	int   fsize;
	int   rsize;
	void *data;
	struct beast_cache_item *next;
};

zend_op_array* (*old_compile_file)(zend_file_handle*, int TSRMLS_DC);

/*
 * authkey, you can change it.
 */
static char authkey[8] = {
	0x01, 0x1f, 0x01, 0x1f,
	0x01, 0x0e, 0x01, 0x0e
};

/* True global resources - no need for thread safety here */
static int le_beast;
static int max_cache_size = DEFAULT_MAX_CACHE_SIZE;
static int cache_use_mem = 0;
static int cache_threshold = 0;
static HashTable *htable;
static struct beast_cache_item *free_cache_list = NULL;
static int free_cache_len = 0;

/* {{{ beast_functions[]
 *
 * Every user visible function must have an entry in beast_functions[].
 */
zend_function_entry beast_functions[] = {
	PHP_FE(beast_encode_file, NULL)
	{NULL, NULL, NULL}	/* Must be the last line in beast_functions[] */
};
/* }}} */

/* {{{ beast_module_entry
 */
zend_module_entry beast_module_entry = {
#if ZEND_MODULE_API_NO >= 20010901
	STANDARD_MODULE_HEADER,
#endif
	"beast",
	beast_functions,
	PHP_MINIT(beast),
	PHP_MSHUTDOWN(beast),
	PHP_RINIT(beast),		/* Replace with NULL if there's nothing to do at request start */
	PHP_RSHUTDOWN(beast),	/* Replace with NULL if there's nothing to do at request end */
	PHP_MINFO(beast),
#if ZEND_MODULE_API_NO >= 20010901
	"0.2", /* Replace with version number for your extension */
#endif
	STANDARD_MODULE_PROPERTIES
};
/* }}} */

#ifdef COMPILE_DL_BEAST
ZEND_GET_MODULE(beast)
#endif


struct beast_cache_item *cache_item_alloc()
{
	struct beast_cache_item *item;
	
	if (free_cache_len > 0) {
		item = free_cache_list;
		free_cache_list = free_cache_list->next;
		free_cache_len--;
	} else {
		item = malloc(sizeof(*item));
	}
	return item;
}


void cache_item_free(struct beast_cache_item *item)
{
	if (free_cache_len < FREE_CACHE_LENGTH) { /* cache item */
		item->next = free_cache_list;
		free_cache_list = item;
		free_cache_len++;
	} else {
		free(item);
	}
}

/*****************************************************************************
*                                                                            *
*  Encrypt a plain text file and output a cipher file                        *
*                                                                            *
*****************************************************************************/

int encrypt_file(const char *inputfile, const char *outputfile, const char *key TSRMLS_DC) {
	php_stream *input_stream, *output_stream;
	php_stream_statbuf stat_ssb;
	int fsize, fcount, i;
	char input[8], output[8];
	char header[8];
	
	/* Open input file */
	input_stream = php_stream_open_wrapper(inputfile, "r",
		ENFORCE_SAFE_MODE | REPORT_ERRORS, NULL);
	if (!input_stream) {
		return -1;
	}

	/* Get input file size */
	if (php_stream_stat(input_stream, &stat_ssb)) {
		php_stream_pclose(input_stream);
		return -1;
	}
	fsize = stat_ssb.sb.st_size;

	/* Open output file */
	output_stream = php_stream_open_wrapper(outputfile, "w+",
		ENFORCE_SAFE_MODE | REPORT_ERRORS, NULL);
	if (!output_stream) {
		php_stream_pclose(input_stream);
		return -1;
	}
	
	fcount = fsize / 8 + 1;
	*((int *)&header[0]) = 0xe816a40c;
	*((int *)&header[4]) = fsize;

	php_stream_write(output_stream, header, 8);
	
	for (i = 0; i < fcount; i++)
	{
		memset(input, 0, 8);
		(void)php_stream_read(input_stream, input, 8);
		DES_encipher(input, output, key);
		php_stream_write(output_stream, output, 8);
	}
	
	php_stream_pclose(input_stream);
	php_stream_pclose(output_stream);
	return 0;
}


int beast_clean_cache(char *key, int keyLength, void *value) {
	struct beast_cache_item *item;
	
	if (hash_remove(htable, key, &item) == 0) {
		cache_use_mem -= item->rsize;
		free(item->data); /* free cache data */
		cache_item_free(item);
		if (cache_use_mem <= cache_threshold) {
			return -1; /* break foreach */
		}
	}
	return 0;
}

/*****************************************************************************
*                                                                            *
*  Decrypt a cipher text file and output plain buffer                        *
*                                                                            *
*****************************************************************************/

int decrypt_file_return_buffer(const char *inputfile, const char *key,
        char **buf, int *filesize, int *realsize TSRMLS_DC) {
	php_stream *stream;
	int fsize, bsize, i;
	int allocsize;
	char input[8];
	char header[8];
	char *plaintext, *phpcode;
	
	stream = php_stream_open_wrapper(inputfile, "r",
		ENFORCE_SAFE_MODE | REPORT_ERRORS, NULL);
	if (!stream) {
		return -1;
	}
	
	if ((php_stream_read(stream, header, 8) != 8) ||
	    *((int *)&header[0]) != 0xe816a40c)
	{
		php_stream_pclose(stream);
		return -1;
	}
	
	fsize = *((int *)&header[4]);
	bsize = fsize / 8 + 1; /* block count */
	
	allocsize = bsize * 8 + 3;
	
	if (cache_use_mem + allocsize > max_cache_size) { /* exceed max cache size, free some cache */
		cache_threshold = max_cache_size - allocsize; /* set threshold value */
		hash_foreach(htable, beast_clean_cache);
	}
	
	/* OK, enough memory to alloc caches */
	
	plaintext = malloc(allocsize); /* alloc memory(1) */
	if (!plaintext) {
		php_stream_pclose(stream);
		return -1;
	}
	
	/* For closing php script environment */
	plaintext[0] = ' ';
	plaintext[1] = '?';
	plaintext[2] = '>';
	
	phpcode = &plaintext[3];
	for (i = 0; i < bsize; i++) {
		php_stream_read(stream, input, 8);
		DES_decipher(input, &(phpcode[i * 8]), key);
	}
	
	*buf = plaintext;
	*filesize = fsize + 3;
	*realsize = allocsize;
	
	cache_use_mem += allocsize; /* all caches used memory size */
	
	php_stream_pclose(stream);
	return 0;
}


zend_op_array* 
my_compile_file(zend_file_handle* h, int type TSRMLS_DC)
{
	char *phpcode;
	int filesize, realsize;
	zval pv;
	zend_op_array *new_op_array;
	struct beast_cache_item *cache;
	int nfree = 0;
	
	if (hash_lookup(htable, h->filename, (void **)&cache) == 0) { /* cache exists */
		filesize = cache->fsize;
		phpcode  = cache->data;
	} else { /* cache not exists */
		if (decrypt_file_return_buffer(h->filename, authkey, &phpcode, 
		        &filesize, &realsize TSRMLS_CC) != 0)
		{
			return old_compile_file(h, type TSRMLS_CC);
		}
		
		cache = cache_item_alloc();
		if (!cache) {
			nfree = 1;
		} else {
			cache->fsize = filesize; /* save file size */
			cache->rsize = realsize; /* save real size */
			cache->data = phpcode;   /* save php code */
			if (hash_insert(htable, h->filename, cache) != 0) {
				cache_item_free(cache);
				nfree = 1;
			}
		}
	}
	
	pv.value.str.len = filesize;
	pv.value.str.val = phpcode;
	pv.type = IS_STRING;
	
	new_op_array = compile_string(&pv, "Beast module code" TSRMLS_CC);
	
	if (nfree) {
		free(phpcode); /* free memory(1) */
	}
	
	return new_op_array;
}

/* {{{ PHP_INI
 */
/* Remove comments and fill if you need to have entries in php.ini
PHP_INI_BEGIN()
    STD_PHP_INI_ENTRY("beast.global_value",      "42", PHP_INI_ALL, OnUpdateLong, global_value, zend_beast_globals, beast_globals)
    STD_PHP_INI_ENTRY("beast.global_string", "foobar", PHP_INI_ALL, OnUpdateString, global_string, zend_beast_globals, beast_globals)
PHP_INI_END()
*/

ZEND_INI_MH(php_beast_cache_size) 
{
    if (new_value_length == 0) { 
        return FAILURE;
    }
    
    max_cache_size = atoi(new_value);
    if (max_cache_size <= 0) {
    	max_cache_size = DEFAULT_MAX_CACHE_SIZE;
    }
    
    return SUCCESS;
}

PHP_INI_BEGIN()
    PHP_INI_ENTRY("beast.cache_size", "1048576", PHP_INI_ALL, php_beast_cache_size) 
PHP_INI_END()

/* }}} */

/* {{{ php_beast_init_globals
 */
/* Uncomment this function if you have INI entries
static void php_beast_init_globals(zend_beast_globals *beast_globals)
{
	beast_globals->global_value = 0;
	beast_globals->global_string = NULL;
}
*/
/* }}} */




/* {{{ PHP_MINIT_FUNCTION
 */
PHP_MINIT_FUNCTION(beast)
{
	/* If you have INI entries, uncomment these lines */
	REGISTER_INI_ENTRIES();
	
	old_compile_file = zend_compile_file;
	zend_compile_file = my_compile_file;
	
	htable = hash_alloc(HASH_INIT_SIZE);
	if (!htable) {
	    return FAILURE;
	}

	return SUCCESS;
}
/* }}} */

void beast_destroy_handler(void *data)
{
	struct beast_cache_item *item = data;
	if (item) {
		free(item->data);
		free(item);
	}
}

/* {{{ PHP_MSHUTDOWN_FUNCTION
 */
PHP_MSHUTDOWN_FUNCTION(beast)
{
	/* uncomment this line if you have INI entries */
	UNREGISTER_INI_ENTRIES();

	hash_destroy(htable, &beast_destroy_handler);
	return SUCCESS;
}
/* }}} */

/* Remove if there's nothing to do at request start */
/* {{{ PHP_RINIT_FUNCTION
 */
PHP_RINIT_FUNCTION(beast)
{
	return SUCCESS;
}
/* }}} */

/* Remove if there's nothing to do at request end */
/* {{{ PHP_RSHUTDOWN_FUNCTION
 */
PHP_RSHUTDOWN_FUNCTION(beast)
{
	return SUCCESS;
}
/* }}} */

/* {{{ PHP_MINFO_FUNCTION
 */
PHP_MINFO_FUNCTION(beast)
{
	php_info_print_table_start();
	php_info_print_table_header(2, "beast support", "enabled");
	php_info_print_table_end();

	/* Remove comments if you have entries in php.ini */
	DISPLAY_INI_ENTRIES();
}
/* }}} */


/* Every user-visible function in PHP should document itself in the source */
/* {{{ proto string confirm_beast_compiled(string arg)
   Return a string to confirm that the module is compiled in */
PHP_FUNCTION(beast_encode_file)
{
	char *input, *output;
	char *itmp, *otmp;
	int input_len, output_len;
	int retval;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "ss", &input, 
	        &input_len, &output, &output_len TSRMLS_CC) == FAILURE)
	{
		return;
	}
	
	itmp = malloc(input_len + 1);
	otmp = malloc(output_len + 1);
	if (!itmp || !otmp) {
		if (itmp) free(itmp);
		if (otmp) free(otmp);
		RETURN_FALSE;
	}
	
	memcpy(itmp, input, input_len);
	itmp[input_len] = 0;
	
	memcpy(otmp, output, output_len);
	otmp[output_len] = 0;

	retval = encrypt_file(itmp, otmp, authkey TSRMLS_CC);
	
	free(itmp);
	free(otmp);
	
	if (retval == -1)
		RETURN_FALSE;
	RETURN_TRUE;
}
/* }}} */


/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
