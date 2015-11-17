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

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

typedef struct yy_buffer_state *YY_BUFFER_STATE;

#include "zend.h"
#include "zend_operators.h"
#include "zend_globals.h"
#include "php_globals.h"
#include "zend_language_scanner.h"
#include <zend_language_parser.h>

#include "zend_API.h"
#include "zend_compile.h"

#include "php.h"
#include "php_ini.h"
#include "main/SAPI.h"
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
int beast_ncpu;

/* True global resources - no need for thread safety here */
static zend_op_array* (*old_compile_file)(zend_file_handle*, int TSRMLS_DC);

static int le_beast;
static int max_cache_size = DEFAULT_CACHE_SIZE;
static int cache_hits = 0;
static int cache_miss = 0;
static int beast_enable = 1;
static int beast_cli_module = 0;

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
    "1.4", /* Replace with version number for your extension */
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


int filter_code_comments(char *filename, zval *retval)
{
    zend_lex_state original_lex_state;
    zend_file_handle file_handle = {0};

#if PHP_API_VERSION > 20090626

    php_output_start_default(TSRMLS_C);

    file_handle.type = ZEND_HANDLE_FILENAME;
    file_handle.filename = filename;
    file_handle.free_filename = 0;
    file_handle.opened_path = NULL;

    zend_save_lexical_state(&original_lex_state TSRMLS_CC);
    if (open_file_for_scanning(&file_handle TSRMLS_CC) == FAILURE) {
        zend_restore_lexical_state(&original_lex_state TSRMLS_CC);
        php_output_end(TSRMLS_C);
        return -1;
    }

    zend_strip(TSRMLS_C);

    zend_destroy_file_handle(&file_handle TSRMLS_CC);
    zend_restore_lexical_state(&original_lex_state TSRMLS_CC);

    php_output_get_contents(retval TSRMLS_CC);
    php_output_discard(TSRMLS_C);

#else

    file_handle.type = ZEND_HANDLE_FILENAME;
    file_handle.filename = filename;
    file_handle.free_filename = 0;
    file_handle.opened_path = NULL;

    zend_save_lexical_state(&original_lex_state TSRMLS_CC);
    if (open_file_for_scanning(&file_handle TSRMLS_CC) == FAILURE) {
        zend_restore_lexical_state(&original_lex_state TSRMLS_CC);
        return -1;
    }

    php_start_ob_buffer(NULL, 0, 1 TSRMLS_CC);

    zend_strip(TSRMLS_C);

    zend_destroy_file_handle(&file_handle TSRMLS_CC);
    zend_restore_lexical_state(&original_lex_state TSRMLS_CC);

    php_ob_get_buffer(retval TSRMLS_CC);
    php_end_ob_buffer(0, 0 TSRMLS_CC);

#endif

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
    php_stream *output_stream;
    int file_size, block_count, i, fill;
    char input[8], output[8];
    char header[8];
    zval codes;
    char *codes_str;

    /* Get php codes from script file */
    if (filter_code_comments((char *)inputfile, &codes) == -1) {
        php_error_docref(NULL TSRMLS_CC, E_ERROR,
                              "Unable get codes from php file `%s'", inputfile);
        return -1;
    }

    file_size = codes.value.str.len;
    codes_str = codes.value.str.val;

    /* Open output file */
    output_stream = php_stream_open_wrapper((char *)outputfile, "w+",
        ENFORCE_SAFE_MODE | REPORT_ERRORS, NULL);
    if (!output_stream) {
        php_error_docref(NULL TSRMLS_CC, E_ERROR,
                                        "Unable to open file `%s'", outputfile);
        return -1;
    }

    /* How many DES blocks */
    if ((file_size % 8) == 0) {
        fill = 0;
        block_count = file_size / 8;
    } else {
        fill = file_size % 8;
        block_count = file_size / 8 + 1;
    }

    header[0] = CHR1;
    header[1] = CHR2;
    header[2] = CHR3;
    header[3] = CHR4;

    /* if computer is little endian, change file size to big endian */
    if (little_endian()) {
        file_size = swab32(file_size);
    }

    /* Save file size into encrypt file */
    *((int *)&header[4]) = file_size;

    php_stream_write(output_stream, header, 8);

    for (i = 0; i < block_count; i++) {
        memset(input, 0, 8);

        if (i + 1 == block_count && fill > 0) { /* The last block not enough 8 bytes */
            memcpy(input, &codes_str[i * 8], fill);
        } else {
            memcpy(input, &codes_str[i * 8], 8);
        }

        DES_encipher(input, output, key);
        php_stream_write(output_stream, output, 8);
    }

    php_stream_close(output_stream);
    zval_dtor(&codes);

    return 0;
}


/*****************************************************************************
*                                                                            *
*  Decrypt a cipher text file and output plain buffer                        *
*                                                                            *
*****************************************************************************/

int decrypt_file_return_buffer(int stream, const char *key,
    char **retbuf, int *rsize, int *need_free TSRMLS_DC)
{
    struct stat stat_ssb;
    cache_key_t ckey;
    cache_item_t *citem;
    int file_size, block_size, mem_size, i, n;
    char input[8];
    char header[8];
    char *buffer, *script;

    if (fstat(stream, &stat_ssb) == -1) {
        return -1;
    }

    /* here not set file size because find cache not need file size */
    ckey.device = stat_ssb.st_dev;
    ckey.inode  = stat_ssb.st_ino;
    ckey.mtime  = stat_ssb.st_mtime;

    citem = beast_cache_find(&ckey); /* find cache */

    if (citem != NULL) {  /* found cache, return */
        *retbuf = beast_cache_cdata(citem);
        *rsize  = beast_cache_fsize(citem) + 3; /* includes " ?>" */
        *need_free = 0;
        cache_hits++;
        return 0;
    }

    /* Not found cache */

    if (read(stream, header, 8) != 8 ||
          (header[0] & 0xFF) != CHR1 || (header[1] & 0xFF) != CHR2 ||
          (header[2] & 0xFF) != CHR3 || (header[3] & 0xFF) != CHR4)
    {
        return -1;
    }

    file_size = *((int *)&header[4]); /* Get file size */

    /* If computer is little endian, change file size to little endian */
    if (little_endian()) {
        file_size = swab32(file_size);
    }

    /* Compute DES blocks */
    if (file_size % 8 == 0) {
        block_size = file_size / 8;   
    } else {
        block_size = file_size / 8 + 1;
    }

    mem_size = block_size * 8 + 3;  /* How many memory would alloc */
    ckey.fsize = file_size;

    citem = beast_cache_create(&ckey, mem_size);
    if (NULL == citem) {
        if (NULL == (buffer = malloc(mem_size))) {
            php_error_docref(NULL TSRMLS_CC, E_ERROR, "Out of memory");
            return -1;
        }
        *need_free = 1; /* must be free when compile the string */

    } else {
        buffer = beast_cache_cdata(citem);
        *need_free = 0;
    }

    /* Closing php script environment " ?>" */
    buffer[0] = ' ';
    buffer[1] = '?';
    buffer[2] = '>';

    /* Starting decrypt file */

    script = &buffer[3];

    for (i = 0; i < block_size; i++) {
        n = read(stream, input, 8);
        DES_decipher(input, &(script[i * 8]), key);
    }

    if (NULL != citem) {
        citem   = beast_cache_push(citem); /* push into cache item to manager */
        *retbuf = beast_cache_cdata(citem);
        *rsize  = beast_cache_fsize(citem) + 3;

    } else {
        *retbuf = buffer;
        *rsize  = file_size + 3;
    }

    cache_miss++;

    return 0;
}


/*
 * used cache
 */
zend_op_array * 
cgi_compile_file(zend_file_handle *h, int type TSRMLS_DC)
{
    char *buffer, *file_path, *opened_path;
    int fd = -1;
    FILE *fp = NULL;
    int file_size;
    int need_free = 0;
    zval pv;
    zend_op_array *new_op_array;
    int dummy = 1;

    /* Open the file */
    fp = zend_fopen(h->filename, &opened_path TSRMLS_CC);
    if (fp != NULL) {
        fd = fileno(fp);
    }

    /* Decrypt */
    if (fp == NULL || decrypt_file_return_buffer(fd, __authkey, &buffer,
        &file_size, &need_free TSRMLS_CC) != 0)
    {
        if (fp != NULL) {
            fclose(fp);
        }
        return old_compile_file(h, type TSRMLS_CC);
    }

    /*
     * close file descriptor when beast decrypt success
     */
    if (h->type == ZEND_HANDLE_FP) fclose(h->handle.fp);
    if (h->type == ZEND_HANDLE_FD) close(h->handle.fd);

    if (h->opened_path) {
        file_path = h->opened_path;
    } else {
        file_path = h->filename;
    }

    pv.value.str.len = file_size;
    pv.value.str.val = buffer;
    pv.type = IS_STRING;

    new_op_array = compile_string(&pv, file_path TSRMLS_CC);

#if 0
    if (new_op_array) {
        /* add current file to included_files */
        zend_hash_add(&EG(included_files), file_path, strlen(file_path)+1,
            (void *)&dummy, sizeof(int), NULL);
    }
#endif

    if (need_free) {
        free(buffer);
    }

    fclose(fp);

    return new_op_array;
}


/*
 * don't used cache
 */
zend_op_array *
cli_compile_file(zend_file_handle *h, int type TSRMLS_DC)
{
    int stream;
    FILE *fp = NULL;
    int file_size, block_size, mem_size, i;
    char input[8];
    char header[8];
    char *buffer, *script, *file_path, *opened_path;
    zval pv;
    zend_op_array *new_op_array;
    int dummy = 1, n;

    fp = zend_fopen(h->filename, &opened_path TSRMLS_CC);
    if (fp != NULL) {
        stream = fileno(fp);
    } else {
        return old_compile_file(h, type TSRMLS_CC);
    }

    if (read(stream, header, 8) != 8 ||
         (header[0] & 0xFF) != CHR1 || (header[1] & 0xFF) != CHR2 ||
         (header[2] & 0xFF) != CHR3 || (header[3] & 0xFF) != CHR4)
    {
        fclose(fp);
        return old_compile_file(h, type TSRMLS_CC);
    }

    file_size = *((int *)&header[4]);

    /* if computer is little endian, change file size to little endian */

    if (little_endian()) {
        file_size = swab32(file_size);
    }

    block_size = file_size / 8 + 1; /* block count */
    mem_size = block_size * 8 + 3; /* how many memorys would alloc */

    if (NULL == (buffer = malloc(mem_size))) {
        fclose(fp);
        php_error_docref(NULL TSRMLS_CC, E_ERROR, "Out of memory");
        return old_compile_file(h, type TSRMLS_CC);
    }

    /* closing php script environment " ?>" */

    buffer[0] = ' ';
    buffer[1] = '?';
    buffer[2] = '>';

    script = &buffer[3];

    for (i = 0; i < block_size; i++) {
        n = read(stream, input, 8);
        DES_decipher(input, &(script[i * 8]), __authkey);
    }

    fclose(fp);

    /* finish decrypt file, do compile */

    if (h->opened_path) {
        file_path = h->opened_path;
    } else {
        file_path = h->filename;
    }

    /* close file descriptor when beast decrypt success */
    if (h->type == ZEND_HANDLE_FP) fclose(h->handle.fp);
    if (h->type == ZEND_HANDLE_FD) close(h->handle.fd);

    pv.value.str.len = file_size + 3;
    pv.value.str.val = buffer;
    pv.type = IS_STRING;

    new_op_array = compile_string(&pv, file_path TSRMLS_CC);

#if 0
    if (new_op_array) {
        /* add current file to included_files */
        zend_hash_add(&EG(included_files), file_path, strlen(file_path)+1,
            (void *)&dummy, sizeof(int), NULL);
    }
#endif

    free(buffer);

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


ZEND_INI_MH(php_beast_enable)
{
    if (new_value_length == 0) {
        return FAILURE;
    }

    if (!strcasecmp(new_value, "on") || !strcmp(new_value, "1")) {
        beast_enable = 1;
    } else {
        beast_enable = 0;
    }

    return SUCCESS;
}


PHP_INI_BEGIN()
    PHP_INI_ENTRY("beast.cache_size", "1048576", PHP_INI_ALL,
          php_beast_cache_size)
    PHP_INI_ENTRY("beast.log_file", "/tmp/beast.log", PHP_INI_ALL,
          php_beast_log_file)
    PHP_INI_ENTRY("beast.enable", "1", PHP_INI_ALL,
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

    if (!strcasecmp(sapi_module.name, "cli")) { /* cli module */
        beast_cli_module = 1;
    }

    if (!beast_cli_module) {

        if (beast_cache_init(max_cache_size) == -1 ||
            beast_log_init(beast_log_file) == -1)
        {
            php_error_docref(NULL TSRMLS_CC, E_ERROR,
                                           "Unable initialize cache for beast");
            return FAILURE;
        }
    
        old_compile_file = zend_compile_file;
        zend_compile_file = cgi_compile_file;
    
        beast_ncpu = sysconf(_SC_NPROCESSORS_ONLN); /* Get CPU nums */
        if (beast_ncpu <= 0) {
            beast_ncpu = 1;
        }

    } else {
        old_compile_file = zend_compile_file;
        zend_compile_file = cli_compile_file;
    }

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

    if (!beast_cli_module) {
        beast_cache_destroy();
        beast_log_destroy();
    }

    zend_compile_file = old_compile_file;

    return SUCCESS;
}
/* }}} */

/* {{{ PHP_RINIT_FUNCTION
 */
PHP_RINIT_FUNCTION(beast)
{
    return SUCCESS;
}
/* }}} */

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
    php_info_print_table_header(2, "beast V1.3 support", "enabled");
    php_info_print_table_end();

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

