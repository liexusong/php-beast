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
#include "beast_log.h"


#define DEFAULT_CACHE_SIZE  1048576


extern char __authkey[];

/*
 * Global vaiables for extension
 */

char *beast_log_file;
char *beast_lock_path;

/* True global resources - no need for thread safety here */
static zend_op_array* (*old_compile_file)(zend_file_handle*, int TSRMLS_DC);

static int le_beast;
static int max_cache_size = DEFAULT_CACHE_SIZE;
static int cache_hits = 0;
static int cache_miss = 0;
static int beast_enable = 1;

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
    PHP_RINIT(beast),
    PHP_RSHUTDOWN(beast),
    PHP_MINFO(beast),
#if ZEND_MODULE_API_NO >= 20010901
    "0.5", /* Replace with version number for your extension */
#endif
    STANDARD_MODULE_PROPERTIES
};
/* }}} */

#ifdef COMPILE_DL_BEAST
ZEND_GET_MODULE(beast)
#endif


#define CHR1  0xe8
#define CHR2  0x16
#define CHR3  0xa4
#define CHR4  0x0c

#define swab32(x)                                        \
     ((x & 0x000000FF) << 24 | (x & 0x0000FF00) << 8 |   \
      (x & 0x00FF0000) >>  8 | (x & 0xFF000000) >> 24)


#define little_endian()  (!big_endian())

static int big_endian()
{
    unsigned short num = 0x1122;

    if(*((unsigned char *)&num) == 0x11) {
        return 1;
    }
    return 0;
}


/*****************************************************************************
*                                                                            *
*  Encrypt a plain text file and output a cipher file                        *
*                                                                            *
*****************************************************************************/

int encrypt_file(const char *inputfile, const char *outputfile, 
    const char *key TSRMLS_DC)
{
    php_stream *input_stream, *output_stream;
    php_stream_statbuf stat_ssb;
    int file_size, block_count, i;
    char input[8], output[8];
    char header[8];

    /* Open input file */
    input_stream = php_stream_open_wrapper(inputfile, "r",
        ENFORCE_SAFE_MODE | REPORT_ERRORS, NULL);
    if (!input_stream) {
        php_printf("PHP Fatal error: Unable to open `%s'", inputfile);
        return -1;
    }

    /* Get input file size */
    if (php_stream_stat(input_stream, &stat_ssb)) {
        php_stream_pclose(input_stream);
        return -1;
    }

    file_size = stat_ssb.sb.st_size;
    if (file_size <= 0) {
        php_stream_pclose(input_stream);
        return -1;
    }

    /* Open output file */
    output_stream = php_stream_open_wrapper(outputfile, "w+",
        ENFORCE_SAFE_MODE | REPORT_ERRORS, NULL);
    if (!output_stream) {
        php_printf("PHP Fatal error: Unable to open `%s'", outputfile);
        php_stream_pclose(input_stream);
        return -1;
    }

    block_count = file_size / 8 + 1;

    header[0] = CHR1;
    header[1] = CHR2;
    header[2] = CHR3;
    header[3] = CHR4;

    /* if computer is little endian, change file size to big endian */

    if (little_endian()) {
        file_size = swab32(file_size);
    }

    *((int *)&header[4]) = file_size;

    php_stream_write(output_stream, header, 8);

    for (i = 0; i < block_count; i++)
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

    if (php_stream_read(stream, header, 8) != 8 ||
         (header[0] & 0xFF) != CHR1 || (header[1] & 0xFF) != CHR2 ||
         (header[2] & 0xFF) != CHR3 || (header[3] & 0xFF) != CHR4)
    {
        php_stream_pclose(stream);
        return -1;
    }

    fsize = *((int *)&header[4]);

    /* if computer is little endian, change file size to little endian */

    if (little_endian()) {
        fsize = swab32(fsize);
    }

    bsize = fsize / 8 + 1; /* block count */
    msize = bsize * 8 + 3;

    ckey.fsize = fsize; /* set file size */

    citem = beast_cache_create(&ckey, msize);
    if (!citem) {
        php_stream_pclose(stream);
        beast_write_log(beast_log_error, "Unable alloc memory for cache");
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
    
    if (decrypt_file_return_buffer(h->filename, __authkey, &script, 
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


/* Configure entries */

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

    beast_atoi(new_value, &max_cache_size, &len);

    if (len > 0 && len < new_value_length) { /* have unit */
        switch (new_value[len]) {
        case 'k':
        case 'K':
            max_cache_size *= 1024;
            break;
        case 'm':
        case 'M':
            max_cache_size *= 1024 * 1024;
            break;
        case 'g':
        case 'G':
            max_cache_size *= 1024 * 1024 * 1024;
            break;
        default:
            return FAILURE;
        }

    } else if (len == 0) { /*failed */
        return FAILURE;
    }

    return SUCCESS;
}

ZEND_INI_MH(php_beast_log_file)
{
    if (new_value_length == 0) {
        return FAILURE;
    }

    beast_log_file = strdup(new_value);
    if (beast_log_file == NULL) {
        return FAILURE;
    }

    return SUCCESS;
}

ZEND_INI_MH(php_beast_lock_path)
{
    if (new_value_length == 0) {
        return FAILURE;
    }

    beast_lock_path = strdup(new_value);
    if (beast_lock_path == NULL) {
        return FAILURE;
    }

    return SUCCESS;
}

ZEND_INI_MH(php_beast_enable)
{
    if (new_value_length == 0) {
        return FAILURE;
    }

    if (!strcasecmp(new_value, "on")) {
        beast_enable = 1;
    } else {
        beast_enable = 0;
    }

    return SUCCESS;
}


PHP_INI_BEGIN()
    PHP_INI_ENTRY("beast.cache_size", "1048576", PHP_INI_ALL,
          php_beast_cache_size)
    PHP_INI_ENTRY("beast.log_file", "/home/beast.log", PHP_INI_ALL,
          php_beast_log_file)
    PHP_INI_ENTRY("beast.lock_path", "/tmp/", PHP_INI_ALL,
          php_beast_lock_path)
    PHP_INI_ENTRY("beast.enable", "On", PHP_INI_ALL,
          php_beast_enable)
PHP_INI_END()

/* }}} */


/* {{{ PHP_MINIT_FUNCTION
 */
PHP_MINIT_FUNCTION(beast)
{
    /* If you have INI entries, uncomment these lines */
    REGISTER_INI_ENTRIES();

    if (!beast_enable) {
        return SUCCESS;
    }

    if (beast_cache_init(max_cache_size) == -1) {
        beast_write_log(beast_log_error, "Unable initialize cache for beast");
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
    
    if (!beast_enable) {
        return SUCCESS;
    }

    beast_cache_destroy();

    zend_compile_file = old_compile_file;

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


/* {{{ PHP extern functions */

PHP_FUNCTION(beast_encode_file)
{
    char *input, *output;
    char *itmp, *otmp;
    int input_len, output_len;
    int retval;

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "ss", &input, 
            &input_len, &output, &output_len TSRMLS_CC) == FAILURE)
    {
        RETURN_FALSE;
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

    retval = encrypt_file(itmp, otmp, __authkey TSRMLS_CC);

    free(itmp);
    free(otmp);

    if (retval == -1) {
        RETURN_FALSE;
    }

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
