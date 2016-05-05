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
#include "beast_log.h"
#include "beast_module.h"


#define BEAST_VERSION  "2.0"
#define DEFAULT_CACHE_SIZE  1048576


extern struct beast_ops *ops_handler_list[];

/*
 * Global vaiables for extension
 */
char *beast_log_file;
int beast_ncpu;

/* True global resources - no need for thread safety here */
static zend_op_array* (*old_compile_file)(zend_file_handle*, int TSRMLS_DC);

static struct beast_ops *ops_head = NULL;

static int le_beast;
static int max_cache_size = DEFAULT_CACHE_SIZE;
static int cache_hits = 0;
static int cache_miss = 0;
static int beast_enable = 1;
static char *default_ops_name = NULL;
static int beast_max_filesize = 0;

/* {{{ beast_functions[]
 *
 * Every user visible function must have an entry in beast_functions[].
 */
zend_function_entry beast_functions[] = {
    PHP_FE(beast_encode_file,      NULL)
    PHP_FE(beast_avail_cache,      NULL)
    PHP_FE(beast_cache_info,       NULL)
    PHP_FE(beast_support_filesize, NULL)
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
    BEAST_VERSION, /* Replace with version number for your extension */
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


int beast_register_ops(struct beast_ops *ops)
{
    if (!ops->name || !ops->encrypt || !ops->decrypt) {
        return -1;
    }

    ops->next = ops_head;
    ops_head = ops;

    return 0;
}


struct beast_ops *beast_get_default_ops()
{
    static struct beast_ops *default_ops = NULL;

    if (default_ops) {
        return default_ops;
    }

    default_ops = ops_head;

    while (default_ops) {
        if (!strcasecmp(default_ops->name, default_ops_name)) {
            return default_ops;
        }
        default_ops = default_ops->next;
    }

    /* not found and set first handler */
    default_ops = ops_head;

    return default_ops;
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

int encrypt_file(const char *inputfile, const char *outputfile TSRMLS_DC)
{
    php_stream *output_stream = NULL;
    char header[8];
    zval codes;
    int need_free_code = 0;
    char *inbuf, *outbuf;
    int inlen, outlen;
    struct beast_ops *ops = beast_get_default_ops();

    /* Get php codes from script file */
    if (filter_code_comments((char *)inputfile, &codes) == -1) {
        php_error_docref(NULL TSRMLS_CC, E_ERROR,
                              "Unable get codes from php file `%s'", inputfile);
        return -1;
    }

    need_free_code = 1;

    inlen = codes.value.str.len;
    inbuf = codes.value.str.val;

    /* PHP file size can not large than beast_max_filesize */
    if (inlen > beast_max_filesize) {
        return -1;
    }

    /* Open output file */
    output_stream = php_stream_open_wrapper((char *)outputfile, "w+",
        ENFORCE_SAFE_MODE | REPORT_ERRORS, NULL);
    if (!output_stream) {
        php_error_docref(NULL TSRMLS_CC, E_ERROR,
                                        "Unable to open file `%s'", outputfile);
        goto failed;
    }

    header[0] = CHR1;
    header[1] = CHR2;
    header[2] = CHR3;
    header[3] = CHR4;

    /* if computer is little endian, change file size to big endian */
    if (little_endian()) {
        *((int *)&header[4]) = swab32(inlen);
    } else {
        *((int *)&header[4]) = inlen;
    }

    php_stream_write(output_stream, header, 8 * sizeof(char));

    if (ops->encrypt(inbuf, inlen, &outbuf, &outlen) == -1) {
        php_error_docref(NULL TSRMLS_CC, E_ERROR,
                                     "Unable to encrypt file `%s'", outputfile);
        goto failed;
    }

    php_stream_write(output_stream, outbuf, outlen);
    php_stream_close(output_stream);
    zval_dtor(&codes);
    if (ops->free)
        ops->free(outbuf);
    return 0;

failed:
    if (output_stream)
        php_stream_close(output_stream);
    if (need_free_code)
        zval_dtor(&codes);
    return -1;
}


/*****************************************************************************
*                                                                            *
*  Decrypt a cipher text file and output plain buffer                        *
*                                                                            *
*****************************************************************************/

int decrypt_file(int stream, char **retbuf,
    int *retlen, int *free_buffer TSRMLS_DC)
{
    struct stat stat_ssb;
    cache_key_t findkey;
    cache_item_t *cache;
    int reallen, bodylen;
    char header[8];
    int headerlen;
    char *buffer = NULL, *decbuf;
    int declen;
    struct beast_ops *ops = beast_get_default_ops();

    *free_buffer = 0; /* set free buffer flag to false */

    if (fstat(stream, &stat_ssb) == -1) {
        goto failed;
    }

    findkey.device = stat_ssb.st_dev;
    findkey.inode = stat_ssb.st_ino;
    findkey.mtime = stat_ssb.st_mtime;

    cache = beast_cache_find(&findkey);

    if (cache != NULL) { /* found!!! */
        *retbuf = beast_cache_data(cache);
        *retlen = beast_cache_size(cache);
        cache_hits++;
        return 0;
    }

    /* not found cache and decrypt file */

    headerlen = 8 * sizeof(char);

    if (read(stream, header, headerlen) != headerlen
        || (header[0] & 0xFF) != CHR1
        || (header[1] & 0xFF) != CHR2
        || (header[2] & 0xFF) != CHR3
        || (header[3] & 0xFF) != CHR4)
    {
        goto failed;
    }

    reallen = *((int *)&header[4]); /* real php script file's size */

    if (little_endian()) {
        reallen = swab32(reallen);
    }

    /**
     * how many bytes would be read from encrypt file,
     * subtract encrypt file's header size,
     * because we had read the header yet.
     */

    bodylen = stat_ssb.st_size - headerlen;

    if (!(buffer = malloc(bodylen))
        || read(stream, buffer, bodylen) != bodylen
        || ops->decrypt(buffer, bodylen, &decbuf, &declen) == -1)
    {
        goto failed;
    }

    free(buffer); /* buffer don't need right now and free it */

    findkey.fsize = reallen;

    /* try add decrypt result to cache */
    if ((cache = beast_cache_create(&findkey))) {

        memcpy(beast_cache_data(cache), decbuf, reallen);

        cache = beast_cache_push(cache); /* push cache into hash table */

        *retbuf = beast_cache_data(cache);
        *retlen = beast_cache_size(cache);

        if (ops->free) {
            ops->free(decbuf);
        }

    } else {
        *retbuf = decbuf;
        *retlen = reallen;
        *free_buffer = 1;
    }

    cache_miss++;

    return 0;

failed:
    if (buffer) {
        free(buffer);
    }

    return -1;
}


/*
 * CGI compile file
 */
zend_op_array *
cgi_compile_file(zend_file_handle *h, int type TSRMLS_DC)
{
    char *opened_path, *buffer;
    int fd;
    FILE *filep = NULL;
    int size, free_buffer = 0, destroy_read_shadow = 1;
    int shadow[2]= {0, 0};
    int retval;
    struct beast_ops *ops;

    filep = zend_fopen(h->filename, &opened_path TSRMLS_CC);
     if (filep != NULL) {
         fd = fileno(filep);
     } else {
        goto final;
     }

    retval = decrypt_file(fd, &buffer, &size, &free_buffer TSRMLS_CC);
    if (retval != 0 || pipe(shadow) != 0) {
        goto final;
    }

    /* write data to shadow file */
    if (write(shadow[1], buffer, size) != size) {
        goto final;
    }

    if (h->type == ZEND_HANDLE_FP) fclose(h->handle.fp);
    if (h->type == ZEND_HANDLE_FD) close(h->handle.fd);

    h->type = ZEND_HANDLE_FD;
    h->handle.fd = shadow[0];

    /**
     * zend_compile_file() function would using this file,
     * so don't destroy it.
     */
    destroy_read_shadow = 0;

final:
    if (free_buffer) {
        ops = beast_get_default_ops();
        if (ops && ops->free) {
            ops->free(buffer);
        }
    }

    if (filep) {
        fclose(filep);
    }

    if (shadow[1]) {
        close(shadow[1]);
    }

    if (destroy_read_shadow && shadow[0]) {
        close(shadow[0]);
    }

    return old_compile_file(h, type TSRMLS_CC);
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

    if (len > 0 && len < new_value_length) {
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


ZEND_INI_MH(php_beast_encrypt_handler)
{
    if (new_value_length == 0) {
        return FAILURE;
    }

    default_ops_name = strdup(new_value);
    if (default_ops_name == NULL) {
        return FAILURE;
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
    PHP_INI_ENTRY("beast.encrypt_handler", "des-algo", PHP_INI_ALL,
          php_beast_encrypt_handler)
PHP_INI_END()

/* }}} */


int set_nonblock(int fd)
{
    int flags;

    if ((flags = fcntl(fd, F_GETFL, 0)) == -1 ||
        fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1)
    {
        return -1;
    }
    return 0;
}

/* {{{ PHP_MINIT_FUNCTION
 */
PHP_MINIT_FUNCTION(beast)
{
    struct beast_ops **ops;
    int fds[2];

    /* If you have INI entries, uncomment these lines */
    REGISTER_INI_ENTRIES();

    if (!beast_enable) {
        return SUCCESS;
    }

    /* Check module support the max file size */
    if (pipe(fds) != 0 || set_nonblock(fds[1]) != 0) {
        return FAILURE;
    }

    while (1) {
        if (write(fds[1], "", 1) != 1)
            break;
        beast_max_filesize++;
    }

    close(fds[0]);
    close(fds[1]);

    if (beast_cache_init(max_cache_size) == -1
        || beast_log_init(beast_log_file) == -1)
    {
        php_error_docref(NULL TSRMLS_CC,
                         E_ERROR, "Unable initialize cache for beast");
        return FAILURE;
    }

    old_compile_file = zend_compile_file;
    zend_compile_file = cgi_compile_file;

    beast_ncpu = sysconf(_SC_NPROCESSORS_ONLN); /* Get CPU nums */
    if (beast_ncpu <= 0) {
        beast_ncpu = 1;
    }

    for (ops = ops_handler_list; *ops; ops++) {
        if (beast_register_ops(*ops) == -1) {
            php_error_docref(NULL TSRMLS_CC, E_ERROR,
                "Failed to register encrypt algorithms `%s'", (*ops)->name);
            return FAILURE;
        }
    }

    if (!ops_head) {
        php_error_docref(NULL TSRMLS_CC,
                         E_ERROR, "Havn't active encrypt algorithms in system");
        return FAILURE;
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

    beast_cache_destroy();
    beast_log_destroy();

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
    php_info_print_table_header(2,
        "beast V" BEAST_VERSION " support", "enabled");
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

    retval = encrypt_file(itmp, otmp TSRMLS_CC);

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


PHP_FUNCTION(beast_support_filesize)
{
    RETURN_LONG(beast_max_filesize);
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