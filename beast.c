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
  | Author: Liexusong <liexusong@qq.com>                                 |
  |         maben <www.maben@foxmail.com>                                |
  +----------------------------------------------------------------------+
*/

/* $Id: header,v 1.16.2.1.2.1 2007/01/01 19:32:09 iliaa Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <time.h>

typedef struct yy_buffer_state *YY_BUFFER_STATE;

#include "zend.h"
#include "zend_operators.h"
#include "zend_globals.h"
#include "zend_language_scanner.h"

#include "zend_API.h"
#include "zend_compile.h"

#include "php.h"
#include "php_main.h"
#include "php_globals.h"
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
#include "file_handler.h"

#ifdef PHP_WIN32
    #include <WinSock2.h>
    #include <Iphlpapi.h>
    #pragma comment(lib, PHP_LIB)
    #pragma comment(lib, "Iphlpapi.lib")
#else
    #include <pwd.h>
    #include <unistd.h>
    #include <execinfo.h>
#endif

#if ZEND_MODULE_API_NO >= 20151012
# define BEAST_RETURN_STRING(str, dup) RETURN_STRING(str)
#else
# define BEAST_RETURN_STRING(str, dup) RETURN_STRING(str, dup)
#endif

#define BEAST_VERSION       "2.7"
#define DEFAULT_CACHE_SIZE  10485760   /* 10MB */
#define HEADER_MAX_SIZE     256
#define INT_SIZE            (sizeof(int))

extern struct beast_ops *ops_handler_list[];

/*
 * Global vaiables for extension
 */
char *beast_log_file = NULL;
char *beast_log_user = NULL;
int log_level = beast_log_notice;
int beast_ncpu = 1;
int beast_is_root = 0;
int beast_pid = -1;

/* True global resources - no need for thread safety here */
static zend_op_array* (*old_compile_file)(zend_file_handle*, int TSRMLS_DC);

static int le_beast;
static int max_cache_size = DEFAULT_CACHE_SIZE;
static int beast_enable = 1;
static int beast_max_filesize = 0;
static char *local_networkcard = NULL;
static int beast_now_time = 0;
static int log_normal_file = 0;
static char *beast_debug_path = NULL;
static int beast_debug_mode = 0;

/* {{{ beast_functions[]
 *
 * Every user visible function must have an entry in beast_functions[].
 */
zend_function_entry beast_functions[] = {
    PHP_FE(beast_encode_file,      NULL)
    PHP_FE(beast_avail_cache,      NULL)
    PHP_FE(beast_support_filesize, NULL)
    PHP_FE(beast_file_expire,      NULL)
    PHP_FE(beast_clean_cache,      NULL)
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

extern struct file_handler tmpfile_handler;
extern struct file_handler pipe_handler;

static struct file_handler *default_file_handler = NULL;
static struct file_handler *file_handlers[] = {
    &tmpfile_handler,
#ifndef PHP_WIN32
    &pipe_handler,
#endif
    NULL
};

extern char encrypt_file_header_sign[];
extern int encrypt_file_header_length;
extern char *file_handler_switch;

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


int filter_code_comments(char *filename, zval *retval TSRMLS_DC)
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
    if (filter_code_comments((char *)inputfile, &codes TSRMLS_CC) == -1) {
        php_error_docref(NULL TSRMLS_CC, E_ERROR,
                         "Unable get codes from php file `%s'", inputfile);
        return -1;
    }

    need_free_code = 1;

#if ZEND_MODULE_API_NO >= 20151012
    inlen = Z_STRLEN(codes);
    inbuf = Z_STRVAL(codes);
#else
    inlen = codes.value.str.len;
    inbuf = codes.value.str.val;
#endif

    /* PHP file size can not large than beast_max_filesize */
    if (beast_max_filesize > 0 && inlen > beast_max_filesize) {
        return -1;
    }

    /* Open output file */
#if ZEND_MODULE_API_NO >= 20151012
    output_stream = php_stream_open_wrapper((char *)outputfile, "w+",
                              IGNORE_URL_WIN|REPORT_ERRORS, NULL);
#else
    output_stream = php_stream_open_wrapper((char *)outputfile, "w+",
                              ENFORCE_SAFE_MODE|REPORT_ERRORS, NULL);
#endif

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

int decrypt_file(const char *filename, int stream,
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
    int n = 0;
    int filesize = 0;

#ifdef PHP_WIN32
    ULARGE_INTEGER ull;
    BY_HANDLE_FILE_INFORMATION fileinfo;
    HANDLE hFile = (HANDLE)_get_osfhandle(stream);
    if (!GetFileInformationByHandle(hFile, &fileinfo)) {
        beast_write_log(beast_log_error,
                "Failed to get file information from file `%s'", filename);
        retval = -1;
        goto failed;
    }
    findkey.device = fileinfo.dwVolumeSerialNumber;
    findkey.inode = fileinfo.nFileIndexHigh * (MAXDWORD + 1) + fileinfo.nFileIndexLow;


    ull.LowPart = fileinfo.ftLastWriteTime.dwLowDateTime;
    ull.HighPart = fileinfo.ftLastWriteTime.dwHighDateTime;

    findkey.mtime =  ull.QuadPart / 10000000ULL - 11644473600ULL;
    filesize = fileinfo.nFileSizeHigh * (MAXDWORD + 1) + fileinfo.nFileSizeLow;

#else
    if (fstat(stream, &stat_ssb) == -1) {
        beast_write_log(beast_log_error,
                "Failed to readed state buffer from file `%s'", filename);
        retval = -1;
        goto failed;
    }
    findkey.device = stat_ssb.st_dev;
    findkey.inode = stat_ssb.st_ino;
    findkey.mtime = stat_ssb.st_mtime;
    filesize = stat_ssb.st_size;
#endif

    /**
     * 1) 1 int is dump length,
     * 2) 1 int is expire time.
     * 3) 1 int is encrypt type.
     */
    headerlen = encrypt_file_header_length + INT_SIZE * 3;

    if (filesize < headerlen) { /* This file is not a encrypt file */
        retval = -1;
        goto failed;
    }

    cache = beast_cache_find(&findkey);

    if (cache != NULL) { /* Found cache */
        *retbuf = beast_cache_data(cache);
        *retlen = beast_cache_size(cache);
        return 0;
    }

    *free_buffer = 0; /* Set free buffer flag to false */

    /*  Not found cache and decrypt file */

    if ((n = read(stream, header, headerlen)) != headerlen) {
        beast_write_log(beast_log_error,
                "Failed to readed header from file `%s', headerlen:%d, readlen:%d", filename, headerlen, n);
        retval = -1;
        goto failed;
    }

    /* Not a encrypted file */
    if (memcmp(header,
               encrypt_file_header_sign,
               encrypt_file_header_length))
    {
        if (log_normal_file) {
            beast_write_log(beast_log_error,
                            "File `%s' isn't a encrypted file", filename);
        }

        retval = -1;
        goto failed;
    }

    /* Real php script file's size */
    reallen = *((int *)&header[encrypt_file_header_length]);
    expire  = *((int *)&header[encrypt_file_header_length + INT_SIZE]);
    entype  = *((int *)&header[encrypt_file_header_length + 2 * INT_SIZE]);

    if (little_endian()) {
        reallen = swab32(reallen);
        expire  = swab32(expire);
        entype  = swab32(entype);
    }

    /* Check file size is vaild */
    if (beast_max_filesize > 0 && reallen > beast_max_filesize) {
        beast_write_log(beast_log_error,
                "File size `%d' out of max size `%d'",
                reallen, beast_max_filesize);
        retval = -1;
        goto failed;
    }

    /* Check file is not expired */
    if (expire > 0 && expire < beast_now_time) {
        beast_write_log(beast_log_error, "File `%s' was expired", filename);
        retval = -2;
        goto failed;
    }

    *ret_encrypt = encrypt_ops = beast_get_encrypt_algo(entype);

    /**
     * How many bytes would be read from encrypt file,
     * subtract encrypt file's header size,
     * because we had read the header yet.
     */

    bodylen = filesize - headerlen;

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

    free(buffer); /* Buffer don't need right now and free it */

    findkey.fsize = reallen; /* How many size would we alloc from cache */

    /* Try to add decrypt result to cache */
    if ((cache = beast_cache_create(&findkey))) {

        memcpy(beast_cache_data(cache), decbuf, reallen);

        cache = beast_cache_push(cache); /* Push cache into hash table */

        *retbuf = beast_cache_data(cache);
        *retlen = beast_cache_size(cache);

        if (encrypt_ops->free) {
            encrypt_ops->free(decbuf);
        }

    } else {  /* Return raw buffer and we need free after PHP finished */

        *retbuf = decbuf;
        *retlen = reallen;

        *free_buffer = 1;
    }

    return 0;

failed:
    if (buffer) {
        free(buffer);
    }

    return retval;
}


int beast_super_mkdir(char *path)
{
    char *head, *last;
    char temp[1024];

    for (head = last = path; *last; last++) {

        if (*last == '/') {

            if (last > head) {

                memset(temp, 0, 1024);
                memcpy(temp, path, last - path);

                if (access(temp, F_OK) == -1) {
                    if (mkdir(temp, 0777) != 0) {
                        beast_write_log(beast_log_error,
                                        "Failed to make new directory `%s'",
                                        temp);
                        return -1;
                    }
                }
            }

            head = last + 1;
        }
    }

    return 0;
}


/*
 * CGI compile file
 */
zend_op_array *
cgi_compile_file(zend_file_handle *h, int type TSRMLS_DC)
{
#if ZEND_MODULE_API_NO >= 20151012
    zend_string *opened_path;
#else
    char *opened_path;
#endif
    char *buf;
    int fd;
    FILE *fp = NULL;
    int size, free_buffer = 0;
    int retval;
    struct beast_ops *ops = NULL;
    int destroy_file_handler = 0;

    fp = zend_fopen(h->filename, &opened_path TSRMLS_CC);
    if (fp != NULL) {
        fd = fileno(fp);
    } else {
        goto final;
    }

    retval = decrypt_file(h->filename, fd, &buf, &size,
                          &free_buffer, &ops TSRMLS_CC);
    if (retval == -2) {
        php_error_docref(NULL TSRMLS_CC, E_ERROR,
            "This program was expired, please contact administrator");
        return NULL;
    }

    if (retval == -1) goto final;  /* Using old_compile_file() */

#if BEAST_DEBUG_MODE

    if (beast_debug_mode && beast_debug_path) {

        if (access(beast_debug_path, F_OK) == 0) {

            char realpath[1024];

            sprintf(realpath, "%s/%s", beast_debug_path, h->filename);

            if (beast_super_mkdir(realpath) == 0) {

                FILE *debug_fp = fopen(realpath, "w+");

                if (debug_fp) {
                    fwrite(buf, size, 1, debug_fp);
                    fclose(debug_fp);
                }
            }
        }
    }

#endif

    if (default_file_handler->open(default_file_handler) == -1 ||
        default_file_handler->write(default_file_handler, buf, size) == -1 ||
        default_file_handler->rewind(default_file_handler) == -1)
    {
        destroy_file_handler = 1;
        goto final;
    }

    if (h->type == ZEND_HANDLE_FP) fclose(h->handle.fp);
    if (h->type == ZEND_HANDLE_FD) close(h->handle.fd);

    /*
     * Get file handler and free context
     */
    switch (default_file_handler->type) {
    case BEAST_FILE_HANDLER_FP:
        h->type = ZEND_HANDLE_FP;
        h->handle.fp = default_file_handler->get_fp(default_file_handler);
        break;
    case BEAST_FILE_HANDLER_FD:
        h->type = ZEND_HANDLE_FD;
        h->handle.fd = default_file_handler->get_fd(default_file_handler);
        break;
    }

final:
    if (free_buffer && ops) {
        if (ops->free) {
            ops->free(buf);
        }
    }

    if (fp) fclose(fp);

    if (destroy_file_handler) {
        default_file_handler->destroy(default_file_handler);
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
        return SUCCESS;
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


ZEND_INI_MH(php_beast_log_user)
{
#if ZEND_MODULE_API_NO >= 20151012

    if (ZSTR_LEN(new_value) == 0) {
        return SUCCESS;
    }

    beast_log_user = estrdup(ZSTR_VAL(new_value));
    if (beast_log_user == NULL) {
        return FAILURE;
    }

    return SUCCESS;

#else

    if (new_value_length == 0) {
        return SUCCESS;
    }

    beast_log_user = strdup(new_value);
    if (beast_log_user == NULL) {
        return FAILURE;
    }

    return SUCCESS;

#endif
}

ZEND_INI_MH(php_beast_log_level)
{
    char *level = NULL;
#if ZEND_MODULE_API_NO >= 20151012

    if (ZSTR_LEN(new_value) == 0) {
        return SUCCESS;
    }

    level = ZSTR_VAL(new_value);

#else

    if (new_value_length == 0) {
        return SUCCESS;
    }

    level = new_value;

#endif
    if (level == NULL) {
        return FAILURE;
    }

    if (strcasecmp(level, "debug") == 0) {
        log_level = beast_log_debug;
    } else if (strcasecmp(level, "notice") == 0) {
        log_level = beast_log_notice;
    } else if (strcasecmp(level, "error") == 0) {
        log_level = beast_log_error;
    } else {
        return FAILURE;
    }

    return SUCCESS;
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


ZEND_INI_MH(php_beast_debug_path)
{
    #if ZEND_MODULE_API_NO >= 20151012

    if (ZSTR_LEN(new_value) == 0) {
        return SUCCESS;
    }

    beast_debug_path = estrdup(ZSTR_VAL(new_value));
    if (beast_debug_path == NULL) {
        return FAILURE;
    }

    return SUCCESS;

#else

    if (new_value_length == 0) {
        return SUCCESS;
    }

    beast_debug_path = strdup(new_value);
    if (beast_debug_path == NULL) {
        return FAILURE;
    }

    return SUCCESS;

#endif
}


ZEND_INI_MH(php_beast_debug_mode)
{
    #if ZEND_MODULE_API_NO >= 20151012

    if (ZSTR_LEN(new_value) == 0) {
        return FAILURE;
    }

    if (!strcasecmp(ZSTR_VAL(new_value), "on")
        || !strcmp(ZSTR_VAL(new_value), "1"))
    {
        beast_debug_mode = 1;
    } else {
        beast_debug_mode = 0;
    }

    return SUCCESS;

#else

    if (new_value_length == 0) {
        return FAILURE;
    }

    if (!strcasecmp(new_value, "on")
        || !strcmp(new_value, "1"))
    {
        beast_debug_mode = 1;
    } else {
        beast_debug_mode = 0;
    }

    return SUCCESS;

#endif
}


PHP_INI_BEGIN()
    PHP_INI_ENTRY("beast.cache_size", "10485760", PHP_INI_ALL,
          php_beast_cache_size)
    PHP_INI_ENTRY("beast.log_file", "./php-beast.log", PHP_INI_ALL,
          php_beast_log_file)
    PHP_INI_ENTRY("beast.log_user", "root", PHP_INI_ALL,
          php_beast_log_user)
    PHP_INI_ENTRY("beast.log_level", "notice", PHP_INI_ALL,
          php_beast_log_level)
    PHP_INI_ENTRY("beast.enable", "1", PHP_INI_ALL,
          php_beast_enable)
    PHP_INI_ENTRY("beast.networkcard", "eth0", PHP_INI_ALL,
          php_beast_set_networkcard)
    PHP_INI_ENTRY("beast.log_normal_file", "0", PHP_INI_ALL,
          php_beast_set_log_normal_file)
#if BEAST_DEBUG_MODE
    PHP_INI_ENTRY("beast.debug_path", "/tmp", PHP_INI_ALL,
          php_beast_debug_path)
    PHP_INI_ENTRY("beast.debug_mode", "0", PHP_INI_ALL,
          php_beast_debug_mode)
#endif
PHP_INI_END()

/* }}} */


void segmentfault_deadlock_fix(int sig)
{
#ifdef PHP_WIN32 // windows not support backtrace
    beast_write_log(beast_log_error, "Segmentation fault and fix deadlock");
#else
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
#endif
    beast_mm_unlock();     /* Maybe lock mm so free here */
    beast_cache_unlock();  /* Maybe lock cache so free here */

    exit(sig);
}


static char *get_mac_address(char *networkcard)
{
#ifdef PHP_WIN32

    // For windows
    ULONG size = sizeof(IP_ADAPTER_INFO);
    int ret, i;
    char *address = NULL;
    char buf[128] = { 0 }, *pos;

    PIP_ADAPTER_INFO pCurrentAdapter = NULL;
    PIP_ADAPTER_INFO pIpAdapterInfo = (PIP_ADAPTER_INFO)malloc(sizeof(*pIpAdapterInfo));
    if (!pIpAdapterInfo) {
        beast_write_log(beast_log_error, "Failed to allocate memory for IP_ADAPTER_INFO");
        return NULL;
    }

    ret = GetAdaptersInfo(pIpAdapterInfo, &size);
    if (ERROR_BUFFER_OVERFLOW == ret) {
        // see ERROR_BUFFER_OVERFLOW https://msdn.microsoft.com/en-us/library/aa365917(VS.85).aspx
        free(pIpAdapterInfo);
        pIpAdapterInfo = (PIP_ADAPTER_INFO)malloc(size);

        ret = GetAdaptersInfo(pIpAdapterInfo, &size);
    }

    if (ERROR_SUCCESS != ret) {
        beast_write_log(beast_log_error, "Failed to get network adapter information");
        free(pIpAdapterInfo);
        return NULL;
    }

    pCurrentAdapter = pIpAdapterInfo;
    do {
        if (strcmp(pCurrentAdapter->AdapterName, networkcard) == 0) {
            for (i = 0, pos = buf; i < pCurrentAdapter->AddressLength; i++, pos += 3) {
                sprintf(pos, "%.2X-", (int)pCurrentAdapter->Address[i]);
            }
            *(--pos) = '\0'; // remove last -
            address = strdup(buf);
            break;
        }
        pCurrentAdapter = pCurrentAdapter->Next;
    } while (pCurrentAdapter);

    free(pIpAdapterInfo);
    return address;

#else

    // For linux / unix
    char netfile[128] = { 0 }, cmd[128] = { 0 }, buf[128] = { 0 };
    FILE *fp;
    char *retbuf, *curr, *last;

    snprintf(netfile, 128, "/sys/class/net/%s/address", networkcard);

    if (access((const char *)netfile, R_OK) != 0) { /* File not exists */
        return NULL;
    }

    snprintf(cmd, 128, "cat %s", netfile);

    fp = popen(cmd, "r");
    if (!fp) {
        return NULL;
    }

    retbuf = fgets(buf, 128, fp);

    for (curr = buf, last = NULL; *curr; curr++) {
        if (*curr != '\n') {
            last = curr;
        }
    }

    if (!last) {
        return NULL;
    }

    for (last += 1; *last; last++) {
        *last = '\0';
    }

    pclose(fp);

    return strdup(buf);

#endif
}

static int validate_networkcard()
{
    extern char *allow_networkcards[];
    char **ptr;
    char *networkcard_start, *networkcard_end;
    int endof_networkcard = 0;
    int active = 0;
    char *address;

    for (ptr = allow_networkcards; *ptr; ptr++, active++);

    if (!active) {
        return 0;
    }

    networkcard_start = networkcard_end = local_networkcard;

    while (1) {
        while (*networkcard_end && *networkcard_end != ',') {
            networkcard_end++;
        }

        if (networkcard_start == networkcard_end) { /* empty string */
            break;
        }

        if (*networkcard_end == ',') {
            *networkcard_end = '\0';
        }
        else {
            endof_networkcard = 1;
        }

        address = get_mac_address(networkcard_start);
        if (address) {
            for (ptr = allow_networkcards; *ptr; ptr++) {
                if (!strcasecmp(address, *ptr)) {
                    free(address); /* release buffer */
                    return 0;
                }
            }
            free(address);
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
    int i;
#ifdef PHP_WIN32
    SYSTEM_INFO info;
#endif

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
    for (i = 0; ; i++) {
        default_file_handler = file_handlers[i];
        if (!default_file_handler ||
            !strcasecmp(file_handler_switch, default_file_handler->name))
        {
            break;
        }
    }

    if (!default_file_handler) {
        return FAILURE;
    }

    beast_max_filesize = default_file_handler->check();
    if (beast_max_filesize == -1) {
        return FAILURE;
    }

    if (beast_cache_init(max_cache_size) == -1) {
        php_error_docref(NULL TSRMLS_CC,
                         E_ERROR, "Unable initialize cache for beast");
        return FAILURE;
    }

    if (beast_log_init(beast_log_file, log_level) == -1) {
        php_error_docref(NULL TSRMLS_CC,
                         E_ERROR, "Unable open log file for beast");
        return FAILURE;
    }

#ifndef PHP_WIN32
    if (getuid() == 0 && beast_log_user) { /* Change log file owner user */
        struct passwd *pwd;

        pwd = getpwnam((const char *)beast_log_user);
        if (!pwd) {
            php_error_docref(NULL TSRMLS_CC,
                             E_ERROR, "Unable get user passwd information");
            return FAILURE;
        }

        if (beast_log_chown(pwd->pw_uid, pwd->pw_gid) != 0) {
            php_error_docref(NULL TSRMLS_CC,
                             E_ERROR, "Unable change log file owner");
            return FAILURE;
        }
    }
#endif

    old_compile_file = zend_compile_file;
    zend_compile_file = cgi_compile_file;

#ifdef PHP_WIN32
    GetSystemInfo(&info);
    beast_ncpu = info.dwNumberOfProcessors;
#else
    beast_ncpu = sysconf(_SC_NPROCESSORS_ONLN); /* Get CPU nums */
#endif
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

    beast_write_log(beast_log_debug, "Beast module was initialized");

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
    if (beast_pid == -1) {
        beast_pid = getpid();
    }

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

    zend_string *input_file;

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "S",
                              &input_file TSRMLS_CC) == FAILURE)
    {
        RETURN_FALSE;
    }

    file     = ZSTR_VAL(input_file);
    file_len = ZSTR_LEN(input_file);

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

    if (expire > 0) {
        string = (char *)php_format_date(format, strlen(format), expire, 1 TSRMLS_CC);
        BEAST_RETURN_STRING(string, 0);
    } else {
        BEAST_RETURN_STRING("+Infinity", 1);
    }

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
    RETURN_LONG(beast_mm_availspace());
}


PHP_FUNCTION(beast_support_filesize)
{
    RETURN_LONG(beast_max_filesize);
}


PHP_FUNCTION(beast_clean_cache)
{
    beast_cache_flush();
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker expandtab
 * vim<600: noet sw=4 ts=4
 */
