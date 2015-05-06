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
  | Author:                                                              |
  +----------------------------------------------------------------------+
*/

/* $Id: header,v 1.16.2.1.2.1 2007/01/01 19:32:09 iliaa Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "php_ini.h"
#include "ext/standard/info.h"
#include "php_beast.h"
#include "encrypt.h"
#include "md5.h"


#define  DEFAULT_MAX_CACHE  5242880  /* 5MB */

/* True global resources - no need for thread safety here */

static int enable_cache = 1;
static int cache_max_size = DEFAULT_MAX_CACHE;
static char *encrypt_filename = NULL;
static int le_beast;

static char __authkey[8] = {
    0x01, 0x1f, 0x01, 0x1f,
    0x01, 0x0e, 0x01, 0x0e
};

/* {{{ beast_functions[]
 *
 * Every user visible function must have an entry in beast_functions[].
 */
zend_function_entry beast_functions[] = {
    PHP_FE(beast_encode_file, NULL)
    PHP_FE(beast_run_file,    NULL)
    PHP_FE(beast_cache_list,  NULL)
    PHP_FE(beast_cache_flush, NULL)
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
    PHP_RINIT(beast),
    PHP_RSHUTDOWN(beast),
    PHP_MINFO(beast),
#if ZEND_MODULE_API_NO >= 20010901
    "0.1", /* Replace with version number for your extension */
#endif
    STANDARD_MODULE_PROPERTIES
};
/* }}} */

#ifdef COMPILE_DL_BEAST
ZEND_GET_MODULE(beast)
#endif



void beast_atoi(const char *str, int *ret, int *len)
{
    const char *ptr = str;
    char ch;
    int absolute = 1;
    int rlen, result;

    ch = *ptr;

    if (ch == '-') {
        absolute = -1;
        ++ptr;
    } else if (ch == '+') {
        absolute = 1;
        ++ptr;
    }

    for (rlen = 0, result = 0; *ptr != '\0'; ptr++) {
        ch = *ptr;

        if (ch >= '0' && ch <= '9') {
            result = result * 10 + (ch - '0');
            rlen++;
        } else {
            break;
        }
    }

    if (ret) *ret = absolute * result;
    if (len) *len = rlen;
}

ZEND_INI_MH(php_beast_cache_size) 
{
    int len;

    if (new_value_length == 0) { 
        return FAILURE;
    }

    beast_atoi(new_value, &cache_max_size, &len);

    if (len > 0 && len < (int)new_value_length) { /* have unit */
        switch (new_value[len]) {
        case 'k':
        case 'K':
            cache_max_size *= 1024;
            break;
        case 'm':
        case 'M':
            cache_max_size *= 1024 * 1024;
            break;
        case 'g':
        case 'G':
            cache_max_size *= 1024 * 1024 * 1024;
            break;
        default:
            return FAILURE;
        }

    } else if (len == 0) { /*failed */
        return FAILURE;
    }

    return SUCCESS;
}


ZEND_INI_MH(php_beast_enable_cache)
{
    int enable = 1, len;

    if (new_value_length == 0) {
        return FAILURE;
    }

    beast_atoi(new_value, &enable, &len);

    enable_cache = enable;

    return SUCCESS;
}


ZEND_INI_MH(php_beast_encrypt_file)
{
    if (new_value_length == 0) {
        return FAILURE;
    }

    encrypt_filename = malloc(new_value_length + 1);
    if (!encrypt_filename) {
        return FAILURE;
    }

    memcpy(encrypt_filename, new_value, new_value_length);
    encrypt_filename[new_value_length] = '\0';

    return SUCCESS;
}


PHP_INI_BEGIN()
    PHP_INI_ENTRY("beast.enable_cache", "1", PHP_INI_ALL,
          php_beast_enable_cache)
    PHP_INI_ENTRY("beast.cache_size", "5242880", PHP_INI_ALL,
          php_beast_cache_size)
    PHP_INI_ENTRY("beast.encrypt_file", NULL, PHP_INI_ALL,
          php_beast_encrypt_file)
PHP_INI_END()


ZEND_DECLARE_MODULE_GLOBALS(beast);


static void php_beast_globals_ctor(zend_beast_globals *beast_globals TSRMLS_DC)
{
    int i;

    for (i = 0; i < CACHE_BUCKET_SIZE; i++) {
        beast_globals->caches[i] = NULL;
    }

    beast_globals->cache_total_size = 0;
}

static void php_beast_globals_dtor(zend_beast_globals *beast_globals TSRMLS_DC)
{
    cache_item *item, *next;
    int i;

    for (i = 0; i < CACHE_BUCKET_SIZE; i++) {
        item = beast_globals->caches[i];
        while (item) {
            next = item->next;
            free(item);
            item = next;
        }
    }
}


/* {{{ PHP_MINIT_FUNCTION
 */
PHP_MINIT_FUNCTION(beast)
{
	REGISTER_INI_ENTRIES();

#ifdef ZTS
    ts_allocate_id(&beast_globals_id, sizeof(zend_beast_globals),
                    (ts_allocate_ctor)php_beast_globals_ctor,
                    (ts_allocate_dtor)php_beast_globals_dtor);
#else
    php_beast_globals_dtor(&beast_globals TSRMLS_CC);
#endif

    /* encrypt file was set */
    if (encrypt_filename) {
        char buffer[32];
        int i;

        if (md5_file(encrypt_filename, buffer) == 0) {
            for (i = 0; i < 8; i++) {
                __authkey[i] = buffer[i * 4];
            }
        }
    }

    return SUCCESS;
}
/* }}} */

/* {{{ PHP_MSHUTDOWN_FUNCTION
 */
PHP_MSHUTDOWN_FUNCTION(beast)
{

    UNREGISTER_INI_ENTRIES();

#ifndef ZTS
    php_beast_globals_dtor(&beast_globals TSRMLS_CC);
#endif

    if (encrypt_filename) {
        free(encrypt_filename);
    }

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

    DISPLAY_INI_ENTRIES();
}
/* }}} */


static long beast_hash(char *key, int klen)
{
    long h = 0, g;
    char *kend = key + klen;

    while (key < kend) {
        h = (h << 4) + *key++;
        if ((g = (h & 0xF0000000))) {
            h = h ^ (g >> 24);
            h = h ^ g;
        }
    }

    return h;
}


/* {{{ */
PHP_FUNCTION(beast_run_file)
{
    char *input_file;
    int input_len, offset;
    php_stream *input_stream;
    php_stream_statbuf stat_ssb;
    cache_item *cache;
    long index;
    int fsize, bsize, msize, i;
    char *buff, *script, input[8];

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "sl", 
                                 &input_file, &input_len, &offset) == FAILURE) {
        return;
    }

    if (enable_cache) {
        /* Find cache */
        index = beast_hash(input_file, input_len) % CACHE_BUCKET_SIZE;
    
        cache = BEAST_G(caches)[index];
        while (cache) {
            if (input_len == cache->fname_size && 
                !memcmp(cache->data, input_file, cache->fname_size))
            {
                break;
            }
            cache = cache->next;
        }

        if (cache) { /* Found cache */
            buff = &cache->data[cache->fname_size + 1];
            goto evalscript;
        }
    }

    /* disable cache or not found cache */

    /* Open encrypt's file */
    input_stream = php_stream_open_wrapper(input_file, "r",
                                       ENFORCE_SAFE_MODE | REPORT_ERRORS, NULL);
    if (!input_stream) {
        php_error_docref(NULL TSRMLS_CC, E_ERROR,
                                        "Unable to open file `%s'", input_file);
        return;
    }

    if (php_stream_stat(input_stream, &stat_ssb)) {
        php_stream_close(input_stream);
        php_error_docref(NULL TSRMLS_CC, E_ERROR,
                                     "Unable get file `%s' status", input_file);
        return;
    }

    /* Get file's size */
    fsize = stat_ssb.sb.st_size;
    msize = fsize - offset;
    bsize = msize / 8;

    if (msize % 8 != 0) { /* must be 8 bytes multiple */
        php_stream_close(input_stream);
        php_error_docref(NULL TSRMLS_CC, E_ERROR,
                                              "Encrypt file format invalid");
        return;
    }

    /* not enough cache? free some cache item */
    if (enable_cache && BEAST_G(cache_total_size) + msize > cache_max_size) {

        cache_item *next;
        int i;

        for (i = 0; i < CACHE_BUCKET_SIZE; i++) {
            cache = BEAST_G(caches)[i];

            while (cache) {
                next = cache->next;

                BEAST_G(cache_total_size) -= cache->cache_size;
                BEAST_G(caches)[i] = next;

                free(cache);

                if (BEAST_G(cache_total_size) + msize <= cache_max_size) {
                    goto enough_memory;
                }
                cache = next;
            }
        }

        php_stream_close(input_stream);
        php_error_docref(NULL TSRMLS_CC, E_ERROR,
                                     "Not enough cache, please setting bigger");
        return;
    }


enough_memory:

    if (enable_cache) {
        cache = malloc(sizeof(cache_item) + input_len + msize + 7);
        if (cache == NULL) {
            php_stream_close(input_stream);
            php_error_docref(NULL TSRMLS_CC, E_ERROR,
                                              "Unable alloc memory for buffer");
            return;
        }

        cache->fname_size = input_len;
        cache->cache_size = msize;

        memcpy(cache->data, input_file, input_len);
        cache->data[input_len] = '\0';

        buff = cache->data + input_len + 1;

    } else {
        buff = malloc(msize + 6);
        if (buff == NULL) {
            php_stream_close(input_stream);
            php_error_docref(NULL TSRMLS_CC, E_ERROR,
                                              "Unable alloc memory for buffer");
            return;
        }
    }

    /*
     * Closing PHP runtime environment and return to text environment
     */
    buff[0] = ' ';
    buff[1] = ';';
    buff[2] = ' ';
    buff[3] = '?';
    buff[4] = '>';

    script = &buff[5];

    php_stream_seek(input_stream, offset, SEEK_SET);

    for (i = 0; i < bsize; i++) {
        (void)php_stream_read(input_stream, input, 8);
        DES_decipher(input, &(script[i * 8]), __authkey);
    }

    buff[msize + 5] = '\0';

    if (enable_cache) { /* insert cache into caches table */
        cache->next = BEAST_G(caches)[index];
        BEAST_G(caches)[index] = cache;
        BEAST_G(cache_total_size) += msize; // incr total cache size
    }

    php_stream_close(input_stream);


evalscript:

    if (zend_eval_string(buff, NULL, input_file TSRMLS_CC) == FAILURE) {
        php_error_docref(NULL TSRMLS_CC, E_ERROR, "Unable execute script string");
    }

    if (!enable_cache) {
        free(buff);
    }

    return;
}
/* }}} */


/*****************************************************************************
*                                                                            *
*  Encrypt a plain text file and output a cipher file                        *
*                                                                            *
*****************************************************************************/

int encrypt_file(char *input_file, char *output_file, const char *key TSRMLS_DC)
{
    php_stream *input_stream, *output_stream;
    php_stream_statbuf stat_ssb;
    int fsize, bsize, i;
    char input[8], output[8];

    /* Open input file */
    input_stream = php_stream_open_wrapper(input_file, "r",
        ENFORCE_SAFE_MODE | REPORT_ERRORS, NULL);
    if (!input_stream) {
        php_error_docref(NULL TSRMLS_CC, E_ERROR,
                                     "Unable to open file `%s'", input_file);
        return -1;
    }

    /* Get input file size */
    if (php_stream_stat(input_stream, &stat_ssb)) {
        php_stream_close(input_stream);
        return -1;
    }

    fsize = stat_ssb.sb.st_size;
    if (fsize <= 0) {
        php_stream_close(input_stream);
        return -1;
    }

    /* Open output file */
    output_stream = php_stream_open_wrapper(output_file, "w+",
        ENFORCE_SAFE_MODE | REPORT_ERRORS, NULL);
    if (!output_stream) {
        php_error_docref(NULL TSRMLS_CC, E_ERROR,
                                     "Unable to open file `%s'", output_file);
        php_stream_close(input_stream);
        return -1;
    }

    php_stream_write(output_stream, OUTPUT_FILE_HEADER,
                                                 sizeof(OUTPUT_FILE_HEADER)-1);

    if (fsize % 8 == 0) {
        bsize = fsize / 8;
    } else {
        bsize = fsize / 8 + 1;
    }

    for (i = 0; i < bsize; i++) {
        memset(input, 0, 8);
        (void)php_stream_read(input_stream, input, 8);
        DES_encipher(input, output, key);
        php_stream_write(output_stream, output, 8);
    }

    php_stream_close(input_stream);
    php_stream_close(output_stream);

    return 0;
}


PHP_FUNCTION(beast_encode_file)
{
    char *input_file = NULL, *output_file = NULL;
    int input_len, output_len;
    char *itmp, *otmp;
    int retval;

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "ss", 
               &input_file, &input_len, &output_file, &output_len) == FAILURE) {
        return;
    }

    itmp = malloc(input_len + 1);
    otmp = malloc(output_len + 1);

    if (!itmp || !otmp) {
        if (itmp) free(itmp);
        if (otmp) free(otmp);
        RETURN_FALSE;
    }

    memcpy(itmp, input_file, input_len);
    itmp[input_len] = '\0';

    memcpy(otmp, output_file, output_len);
    otmp[output_len] = '\0';

    retval = encrypt_file(itmp, otmp, __authkey TSRMLS_CC);

    free(itmp);
    free(otmp);

    if (retval == -1) {
        RETURN_FALSE;
    }

    RETURN_TRUE;
}


PHP_FUNCTION(beast_cache_list)
{
    int i;
    cache_item *item;

    array_init(return_value);

    for (i = 0; i < CACHE_BUCKET_SIZE; i++) {

        item = BEAST_G(caches)[i];

        while (item) {
            add_assoc_long(return_value, item->data, item->cache_size);
            item = item->next;
        }
    }
}


PHP_FUNCTION(beast_cache_flush)
{
    int i;
    cache_item *item, *next;

    for (i = 0; i < CACHE_BUCKET_SIZE; i++) {

        item = BEAST_G(caches)[i];

        while (item) {
            next = item->next;
            free(item);
            item = next;
        }

        BEAST_G(caches)[i] = NULL;
    }

    BEAST_G(cache_total_size) = 0;
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
