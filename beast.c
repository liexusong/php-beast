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
#include "cache.h"
#include "encrypt.h"

/* If you declare any globals in php_beast.h uncomment this:
ZEND_DECLARE_MODULE_GLOBALS(beast)
*/

#define DEFAULT_CACHE_SIZE  5242880

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
static int max_cache_size = DEFAULT_CACHE_SIZE;
static int cache_hits = 0;
static int cache_miss = 0;

/* {{{ beast_functions[]
 *
 * Every user visible function must have an entry in beast_functions[].
 */
zend_function_entry beast_functions[] = {
    PHP_FE(beast_encode_file, NULL)
    PHP_FE(beast_avail_cache, NULL)
    PHP_FE(beast_cache_info,  NULL)
    {NULL, NULL, NULL}    /* Must be the last line in beast_functions[] */
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
    PHP_RINIT(beast),        /* Replace with NULL if there's nothing to do at request start */
    PHP_RSHUTDOWN(beast),    /* Replace with NULL if there's nothing to do at request end */
    PHP_MINFO(beast),
#if ZEND_MODULE_API_NO >= 20010901
    "0.3", /* Replace with version number for your extension */
#endif
    STANDARD_MODULE_PROPERTIES
};
/* }}} */

#ifdef COMPILE_DL_BEAST
ZEND_GET_MODULE(beast)
#endif


/*****************************************************************************
*                                                                            *
*  Encrypt a plain text file and output a cipher file                        *
*                                                                            *
*****************************************************************************/

int encrypt_file(const char *inputfile, const char *outputfile, const char *key TSRMLS_DC)
{
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


/*****************************************************************************
*                                                                            *
*  Decrypt a cipher text file and output plain buffer                        *
*                                                                            *
*****************************************************************************/

int decrypt_file_return_buffer(const char *inputfile, const char *key,
        char **buffer, int *filesize TSRMLS_DC)
{
    php_stream *stream;
    php_stream_statbuf stat_ssb;
    cache_key_t ckey;
    cache_item_t *citem;
    int fsize, bsize, msize, i;
    char input[8];
    char header[8];
    char *text, *script;
    
    stream = php_stream_open_wrapper(inputfile, "r",
        ENFORCE_SAFE_MODE | REPORT_ERRORS, NULL);
    if (!stream) {
        return -1;
    }
    
    if (php_stream_stat(stream, &stat_ssb)) {
        php_stream_pclose(stream);
        return -1;
    }
    
    /* here not set file size because find cache not need file size */
    ckey.device = stat_ssb.sb.st_dev;
    ckey.inode = stat_ssb.sb.st_ino;
    ckey.mtime = stat_ssb.sb.st_mtime;
    
    citem = beast_cache_find(&ckey); /* find cache */
    if (citem) { /* found cache */
        *buffer = beast_cache_cdata(citem);
        *filesize = beast_cache_fsize(citem) + 3;
        php_stream_pclose(stream);
        cache_hits++;
        return 0;
    }
    
    /* not found cache */
    
    if ((php_stream_read(stream, header, 8) != 8) ||
        *((int *)&header[0]) != 0xe816a40c)
    {
        php_stream_pclose(stream);
        return -1;
    }
    
    fsize = *((int *)&header[4]);
    bsize = fsize / 8 + 1; /* block count */
    msize = bsize * 8 + 3;
    
    ckey.fsize = fsize; /* set file size */
    
    citem = beast_cache_create(&ckey, msize);
    if (!citem) {
        php_stream_pclose(stream);
        return -1;
    }
    
    text = beast_cache_cdata(citem);
    
    /* For closing php script environment " ?>" */
    text[0] = ' ';
    text[1] = '?';
    text[2] = '>';
    
    script = &text[3];
    for (i = 0; i < bsize; i++) {
        php_stream_read(stream, input, 8);
        DES_decipher(input, &(script[i * 8]), key);
    }
    
    php_stream_pclose(stream);
    
    citem = beast_cache_push(citem); /* push into cache item to manager */
    
    *buffer = beast_cache_cdata(citem);
    *filesize = beast_cache_fsize(citem) + 3;
    
    cache_miss++;
    
    return 0;
}


zend_op_array* 
my_compile_file(zend_file_handle* h, int type TSRMLS_DC)
{
    char *script, *file_path;
    int fsize;
    zval pv;
    zend_op_array *new_op_array;
    
    if (decrypt_file_return_buffer(h->filename, authkey, &script, 
            &fsize TSRMLS_CC) != 0)
    {
        return old_compile_file(h, type TSRMLS_CC);
    }
    
    if (h->opened_path) {
        file_path = h->opened_path;
    } else {
        file_path = h->filename;
    }
    
    pv.value.str.len = fsize;
    pv.value.str.val = script;
    pv.type = IS_STRING;
    
    new_op_array = compile_string(&pv, file_path TSRMLS_CC);
    
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
        max_cache_size = DEFAULT_CACHE_SIZE;
    }
    
    return SUCCESS;
}

PHP_INI_BEGIN()
    PHP_INI_ENTRY("beast.cache_size", "5242880", PHP_INI_ALL, php_beast_cache_size) 
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
    
    if (beast_cache_init(max_cache_size) == -1) {
        return FAILURE;
    }
    
    old_compile_file = zend_compile_file;
    zend_compile_file = my_compile_file;
    
    return SUCCESS;
}
/* }}} */

/* {{{ PHP_MSHUTDOWN_FUNCTION
 */
PHP_MSHUTDOWN_FUNCTION(beast)
{
    /* uncomment this line if you have INI entries */
    UNREGISTER_INI_ENTRIES();
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


PHP_FUNCTION(beast_avail_cache)
{
    int size = beast_mm_availspace();
    RETURN_LONG(size);
}


PHP_FUNCTION(beast_cache_info)
{
    array_init(return_value);
    beast_cache_info(return_value);
    add_assoc_long(return_value, "cache_hits", cache_hits);
    add_assoc_long(return_value, "cache_miss", cache_miss);
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
