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
#include <signal.h>
#include <time.h>

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
#include "ext/date/php_date.h"
#include "php_streams.h"
#include "php_beast.h"
#include "beast_mm.h"
#include "cache.h"
#include "beast_log.h"
#include "beast_module.h"


#define BEAST_VERSION       "2.5"
#define DEFAULT_CACHE_SIZE  10485760   /* 10MB */
#define HEADER_MAX_SIZE     256
#define INT_SIZE            (sizeof(int))


extern struct beast_ops *ops_handler_list[];

/*
 * Global vaiables for extension
 */
char *beast_log_file = NULL;
int beast_ncpu = 1;

/* True global resources - no need for thread safety here */
static zend_op_array* (*old_compile_file)(zend_file_handle*, int TSRMLS_DC);

static int le_beast;
static int max_cache_size = DEFAULT_CACHE_SIZE;
static int cache_hits = 0;
static int cache_miss = 0;
static int beast_enable = 1;
static int beast_max_filesize = 0;
static char *local_networkcard = NULL;
static int beast_now_time = 0;
static int log_normal_file = 0;

/* {{{ beast_functions[]
 *
 * Every user visible function must have an entry in beast_functions[].
 */
zend_function_entry beast_functions[] = {
    PHP_FE(beast_encode_file,      NULL)
    PHP_FE(beast_avail_cache,      NULL)
    PHP_FE(beast_cache_info,       NULL)
    PHP_FE(beast_support_filesize, NULL)
    PHP_FE(beast_file_expire,      NULL)
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


extern char encrypt_file_header_sign[];
extern int encrypt_file_header_length;

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


struct beast_ops *beast_get_encrypt_algo(int type)
{
    int index = type - 1;

    if (index < 0 || index >= BEAST_ENCRYPT_TYPE_ERROR) {
        return ops_handler_list[0];
    }

    return ops_handler_list[index];
}


/*****************************************************************************
*                                                                            *
*  Encrypt a plain text file and output a cipher file                        *
*                                                                            *
*****************************************************************************/

int encrypt_file(const char *inputfile,
    const char *outputfile, int expire,
    int encrypt_type TSRMLS_DC)
{
    php_stream *output_stream = NULL;
    zval codes;
    int need_free_code = 0;
    char *inbuf, *outbuf;
    int inlen, outlen, dumplen, expireval, dumptype;
    struct beast_ops *encrypt_ops = beast_get_encrypt_algo(encrypt_type);

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
                              ENFORCE_SAFE_MODE|REPORT_ERRORS, NULL);
    if (!output_stream) {
        php_error_docref(NULL TSRMLS_CC, E_ERROR,
                                        "Unable to open file `%s'", outputfile);
        goto failed;
    }

    /* if computer is little endian, change file size to big endian */
    if (little_endian()) {
        dumplen   = swab32(inlen);
        expireval = swab32(expire);
        dumptype  = swab32(encrypt_type);

    } else {
        dumplen   = inlen;
        expireval = expire;
        dumptype  = encrypt_type;
    }

    php_stream_write(output_stream,
        encrypt_file_header_sign, encrypt_file_header_length);
    php_stream_write(output_stream, (const char *)&dumplen, INT_SIZE);
    php_stream_write(output_stream, (const char *)&expireval, INT_SIZE);
    php_stream_write(output_stream, (const char *)&dumptype, INT_SIZE);

    if (encrypt_ops->encrypt(inbuf, inlen, &outbuf, &outlen) == -1) {
        php_error_docref(NULL TSRMLS_CC, E_ERROR,
                         "Unable to encrypt file `%s'", outputfile);
        goto failed;
    }

    php_stream_write(output_stream, outbuf, outlen);
    php_stream_close(output_stream);
    zval_dtor(&codes);

    if (encrypt_ops->free) {
        encrypt_ops->free(outbuf);
    }

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

int decrypt_file(char *filename, int stream,
    char **retbuf, int *retlen, int *free_buffer,
    struct beast_ops **ret_encrypt TSRMLS_DC)
{
    struct stat stat_ssb;
    cache_key_t findkey;
    cache_item_t *cache;
    int reallen, bodylen, expire;
    char header[HEADER_MAX_SIZE];
    int headerlen;
    char *buffer = NULL, *decbuf;
    int declen;
    int entype;
    struct beast_ops *encrypt_ops;
    int retval = -1;

    *free_buffer = 0; /* set free buffer flag to false */

    if (fstat(stream, &stat_ssb) == -1) {
        beast_write_log(beast_log_error,
                      "Failed to readed state buffer from file `%s'", filename);
        retval = -1;
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

    /**
     * 1) 1 int is dump length,
     * 2) 1 int is expire time.
     * 3) 1 int is encrypt type.
     */
    headerlen = encrypt_file_header_length + INT_SIZE * 3;

    if (read(stream, header, headerlen) != headerlen) {
        beast_write_log(beast_log_error,
                        "Failed to readed header from file `%s'", filename);
        retval = -1;
        goto failed;
    }

    /* Not a encrypted file */
    if (memcmp(header, encrypt_file_header_sign, encrypt_file_header_length)) {

        if (log_normal_file) {
            beast_write_log(beast_log_error,
                            "File `%s' isn't a encrypted file", filename);
        }

        retval = -1;
        goto failed;
    }

    /* real php script file's size */
    reallen = *((int *)&header[encrypt_file_header_length]);
    expire  = *((int *)&header[encrypt_file_header_length + INT_SIZE]);
    entype  = *((int *)&header[encrypt_file_header_length + 2 * INT_SIZE]);

    if (little_endian()) {
        reallen = swab32(reallen);
        expire  = swab32(expire);
        entype  = swab32(entype);
    }

    if (expire > 0 && expire < beast_now_time) {
        beast_write_log(beast_log_error, "File `%s' was expired", filename);
        retval = -2;
        goto failed;
    }

    *ret_encrypt = encrypt_ops = beast_get_encrypt_algo(entype);

    /**
     * how many bytes would be read from encrypt file,
     * subtract encrypt file's header size,
     * because we had read the header yet.
     */

    bodylen = stat_ssb.st_size - headerlen;

    /* 1) Alloc memory for decrypt file */
    if (!(buffer = malloc(bodylen))) {
        beast_write_log(beast_log_error,
                        "Failed to alloc memory to file `%s' size `%d'",
                        filename, bodylen);
        retval = -1;
        goto failed;
    }

    /* 2) Read file stream */
    if (read(stream, buffer, bodylen) != bodylen) {
        beast_write_log(beast_log_error,
                        "Failed to readed stream from file `%s'", filename);
        retval = -1;
        goto failed;
    }

    /* 3) Decrypt file stream */
    if (encrypt_ops->decrypt(buffer, bodylen, &decbuf, &declen) == -1) {
        beast_write_log(beast_log_error,
                        "Failed to decrypted file `%s', using `%s' handler",
                        filename, encrypt_ops->name);
        retval = -1;
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

        if (encrypt_ops->free) {
            encrypt_ops->free(decbuf);
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

    return retval;
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
    struct beast_ops *ops = NULL;

    filep = zend_fopen(h->filename, &opened_path TSRMLS_CC);
     if (filep != NULL) {
         fd = fileno(filep);
     } else {
        goto final;
     }

    retval = decrypt_file(h->filename, fd, &buffer, &size,
                          &free_buffer, &ops TSRMLS_CC);
    if (retval == -2) {
        php_error_docref(NULL TSRMLS_CC, E_ERROR,
                      "This program was expired, please contact administrator");
        return NULL;
    }

    if (retval == -1 || pipe(shadow) != 0) {
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
    if (free_buffer && ops) {
        if (ops->free) {
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
#if ZEND_MODULE_API_NO >= 20151012

    char *value = ZSTR_VAL(new_value);
    int length = ZSTR_LEN(new_value);
    int retlen;

    if (length == 0) {
        return FAILURE;
    }

    beast_atoi(value, &max_cache_size, &retlen);

    if (retlen > 0 && retlen < length) {
        switch (value[retlen]) {
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

    } else if (retlen == 0) {
        return FAILURE;
    }

    return SUCCESS;

#else

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

    } else if (len == 0) {
        return FAILURE;
    }

    return SUCCESS;

#endif
}

ZEND_INI_MH(php_beast_log_file)
{
#if ZEND_MODULE_API_NO >= 20151012

    if (ZSTR_LEN(new_value) == 0) {
        return FAILURE;
    }

    beast_log_file = estrdup(ZSTR_VAL(new_value));
    if (beast_log_file == NULL) {
        return FAILURE;
    }

    return SUCCESS;

#else

    if (new_value_length == 0) {
        return SUCCESS;
    }

    beast_log_file = strdup(new_value);
    if (beast_log_file == NULL) {
        return FAILURE;
    }

    return SUCCESS;

#endif
}


ZEND_INI_MH(php_beast_enable)
{
#if ZEND_MODULE_API_NO >= 20151012

    if (ZSTR_LEN(new_value) == 0) {
        return FAILURE;
    }

    if (!strcasecmp(ZSTR_VAL(new_value), "on")
        || !strcmp(ZSTR_VAL(new_value), "1"))
    {
        beast_enable = 1;
    } else {
        beast_enable = 0;
    }

    return SUCCESS;

#else

    if (new_value_length == 0) {
        return FAILURE;
    }

    if (!strcasecmp(new_value, "on")
        || !strcmp(new_value, "1"))
    {
        beast_enable = 1;
    } else {
        beast_enable = 0;
    }

    return SUCCESS;
#endif
}


ZEND_INI_MH(php_beast_set_networkcard)
{
#if ZEND_MODULE_API_NO >= 20151012

    if (ZSTR_LEN(new_value) == 0) {
        return FAILURE;
    }

    local_networkcard = estrdup(ZSTR_VAL(new_value));
    if (local_networkcard == NULL) {
        return FAILURE;
    }

    return SUCCESS;

#else

    if (new_value_length == 0) {
        return FAILURE;
    }

    local_networkcard = strdup(new_value);
    if (local_networkcard == NULL) {
        return FAILURE;
    }

    return SUCCESS;

#endif
}


ZEND_INI_MH(php_beast_set_log_normal_file)
{
#if ZEND_MODULE_API_NO >= 20151012

    if (ZSTR_LEN(new_value) == 0) {
        return FAILURE;
    }

    if (!strcasecmp(ZSTR_VAL(new_value), "on")
        || !strcmp(ZSTR_VAL(new_value), "1"))
    {
        log_normal_file = 1;
    } else {
        log_normal_file = 0;
    }

    return SUCCESS;

#else

    if (new_value_length == 0) {
        return FAILURE;
    }

    if (!strcasecmp(new_value, "on")
        || !strcmp(new_value, "1"))
    {
        log_normal_file = 1;
    } else {
        log_normal_file = 0;
    }

    return SUCCESS;

#endif
}


PHP_INI_BEGIN()
    PHP_INI_ENTRY("beast.cache_size", "10485760", PHP_INI_ALL,
          php_beast_cache_size)
    PHP_INI_ENTRY("beast.log_file", "/tmp/beast.log", PHP_INI_ALL,
          php_beast_log_file)
    PHP_INI_ENTRY("beast.enable", "1", PHP_INI_ALL,
          php_beast_enable)
    PHP_INI_ENTRY("beast.networkcard", "eth0", PHP_INI_ALL,
          php_beast_set_networkcard)
    PHP_INI_ENTRY("beast.log_normal_file", "0", PHP_INI_ALL,
          php_beast_set_log_normal_file)
PHP_INI_END()

/* }}} */


int set_nonblock(int fd)
{
    int flags;

    if ((flags = fcntl(fd, F_GETFL, 0)) == -1
        || fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1)
    {
        return -1;
    }
    return 0;
}


void segmentfault_deadlock_fix(int sig)
{
    void *array[10] = {0};
    size_t size;
    char **info = NULL;
    int i;

    size = backtrace(array, 10);
    info = backtrace_symbols(array, (int)size);

    beast_write_log(beast_log_error, "Segmentation fault and fix deadlock");

    if (info) {
        for (i = 0; i < size; i++) {
            beast_write_log(beast_log_error, info[i]);
        }
        free(info);
    }

    beast_mm_unlock();     /* Maybe lock mm so free here */
    beast_cache_unlock();  /* Maybe lock cache so free here */

    exit(sig);
}


int validate_networkcard()
{
    extern char *allow_networkcards[];
    char **ptr, *curr, *last;
    char *networkcard_start, *networkcard_end;
    int endof_networkcard = 0;
    int active = 0;
    FILE *fp;
    char cmd[128], buf[128];

    for (ptr = allow_networkcards; *ptr; ptr++, active++);

    if (!active) {
        return 0;
    }

    networkcard_start = networkcard_end = local_networkcard;

    while (1) {
        memset(cmd, 0, 128);
        memset(buf, 0, 128);

        while (*networkcard_end && *networkcard_end != ',') {
            networkcard_end++;
        }

        if (networkcard_start == networkcard_end) { /* empty string */
            break;
        }

        if (*networkcard_end == ',') {
            *networkcard_end = '\0';
        } else {
            endof_networkcard = 1;
        }

        snprintf(cmd, 128, "cat /sys/class/net/%s/address", networkcard_start);

        fp = popen(cmd, "r");
        if (!fp) {
            return 0;
        }

        (void)fgets(buf, 128, fp);

        for (curr = buf, last = NULL; *curr; curr++) {
            if (*curr != '\n') {
                last = curr;
            }
        }

        if (!last) {
            return -1;
        }

        for (last += 1; *last; last++) {
            *last = '\0';
        }

        pclose(fp);

        for (ptr = allow_networkcards; *ptr; ptr++) {
            if (!strcasecmp(buf, *ptr)) {
                return 0;
            }
        }

        if (endof_networkcard) {
            break;
        }

        networkcard_start = networkcard_end + 1;
    }

    return -1;
}


/* {{{ PHP_MINIT_FUNCTION
 */
PHP_MINIT_FUNCTION(beast)
{
    int fds[2];

    /* If you have INI entries, uncomment these lines */
    REGISTER_INI_ENTRIES();

    if (!beast_enable) {
        return SUCCESS;
    }

    if (validate_networkcard() == -1) {
        php_error_docref(NULL TSRMLS_CC, E_ERROR,
                         "Not allowed run at this computer");
        return FAILURE;
    }

    if ((encrypt_file_header_length + INT_SIZE * 2) > HEADER_MAX_SIZE) {
        php_error_docref(NULL TSRMLS_CC, E_ERROR,
            "Header size overflow max size `%d'", HEADER_MAX_SIZE);
        return FAILURE;
    }

    /* Check module support the max file size */
    if (pipe(fds) != 0 || set_nonblock(fds[1]) != 0) {
        return FAILURE;
    }

    while (1) {
        if (write(fds[1], "", 1) != 1) {
            break;
        }
        beast_max_filesize++;
    }

    close(fds[0]);
    close(fds[1]);

    if (beast_cache_init(max_cache_size) == -1) {
        php_error_docref(NULL TSRMLS_CC,
                         E_ERROR, "Unable initialize cache for beast");
        return FAILURE;
    }

    if (beast_log_init(beast_log_file) == -1) {
        php_error_docref(NULL TSRMLS_CC,
                         E_ERROR, "Unable open log file for beast");
        return FAILURE;
    }

    old_compile_file = zend_compile_file;
    zend_compile_file = cgi_compile_file;

    beast_ncpu = sysconf(_SC_NPROCESSORS_ONLN); /* Get CPU nums */
    if (beast_ncpu <= 0) {
        beast_ncpu = 1;
    }

    signal(SIGSEGV, segmentfault_deadlock_fix);

    REGISTER_LONG_CONSTANT("BEAST_ENCRYPT_TYPE_DES",
        BEAST_ENCRYPT_TYPE_DES, CONST_CS|CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("BEAST_ENCRYPT_TYPE_AES",
        BEAST_ENCRYPT_TYPE_AES, CONST_CS|CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("BEAST_ENCRYPT_TYPE_BASE64",
        BEAST_ENCRYPT_TYPE_BASE64, CONST_CS|CONST_PERSISTENT);

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
    beast_now_time = time(NULL); /* Update now time */
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


PHP_FUNCTION(beast_file_expire)
{
    char *file;
    int file_len;
    char header[HEADER_MAX_SIZE] = {0};
    int header_len;
    signed long expire = 0;
    int fd = -1;
    char *string;
    char *format = "Y-m-d H:i:s";

#if ZEND_MODULE_API_NO >= 20151012

    zend_string *__file;

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "S",
                              &__file TSRMLS_CC) == FAILURE)
    {
        RETURN_FALSE;
    }

    file     = ZSTR_VAL(__file);
    file_len = ZSTR_LEN(__file);

#else

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s",
                              &file, &file_len TSRMLS_CC) == FAILURE)
    {
        RETURN_FALSE;
    }

#endif

    fd = open(file, O_RDONLY);
    if (fd < 0) {
        goto error;
    }

    header_len = encrypt_file_header_length + INT_SIZE * 2;

    if (read(fd, header, header_len) != header_len
        || memcmp(header, encrypt_file_header_sign, encrypt_file_header_length))
    {
        goto error;
    }

    close(fd);

    expire = *((int *)&header[encrypt_file_header_length + INT_SIZE]);

    if (little_endian()) {
        expire = swab32(expire);
    }

#if ZEND_MODULE_API_NO >= 20151012

    if (expire > 0) {
        string = php_format_date(format, strlen(format), expire, 1 TSRMLS_CC);
        RETURN_STRING(string);
    } else {
        RETURN_STRING("0000-00-00 00:00:00");
    }

#else

    if (expire > 0) {
        string = php_format_date(format, strlen(format), expire, 1 TSRMLS_CC);
        RETURN_STRING(string, 0);
    } else {
        RETURN_STRING("0000-00-00 00:00:00", 1);
    }

#endif

error:
    if (fd >= 0) {
        close(fd);
    }

    RETURN_FALSE;
}


PHP_FUNCTION(beast_encode_file)
{
    char *input, *output;
    int input_len, output_len;
    long expire = 0;
    long encrypt_type = BEAST_ENCRYPT_TYPE_DES;
    int ret;

#if ZEND_MODULE_API_NO >= 20151012

    zend_string *in, *out;

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "SS|ll",
        &in, &out, &expire, &encrypt_type) == FAILURE)
    {
        RETURN_FALSE;
    }

    input      = ZSTR_VAL(in);
    output     = ZSTR_VAL(out);
    input_len  = ZSTR_LEN(in);
    output_len = ZSTR_LEN(out);

#else

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "ss|ll",
            &input, &input_len, &output, &output_len,
            &expire, &encrypt_type TSRMLS_CC) == FAILURE)
    {
        RETURN_FALSE;
    }

#endif

    if (encrypt_type <= 0
        || encrypt_type >= BEAST_ENCRYPT_TYPE_ERROR)
    {
        RETURN_FALSE;
    }

    ret = encrypt_file(input, output,
                      (int)expire, (int)encrypt_type TSRMLS_CC);
    if (ret == -1) {
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

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
