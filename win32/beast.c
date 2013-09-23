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


/* True global resources - no need for thread safety here */
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
	"0.1", /* Replace with version number for your extension */
#endif
	STANDARD_MODULE_PROPERTIES
};
/* }}} */

#ifdef COMPILE_DL_BEAST
ZEND_GET_MODULE(beast)
#endif


/* {{{ PHP_MINIT_FUNCTION
 */
PHP_MINIT_FUNCTION(beast)
{
	/* If you have INI entries, uncomment these lines 
	REGISTER_INI_ENTRIES();
	*/
	return SUCCESS;
}
/* }}} */

/* {{{ PHP_MSHUTDOWN_FUNCTION
 */
PHP_MSHUTDOWN_FUNCTION(beast)
{
	/* uncomment this line if you have INI entries
	UNREGISTER_INI_ENTRIES();
	*/
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

	/* Remove comments if you have entries in php.ini
	DISPLAY_INI_ENTRIES();
	*/
}
/* }}} */



/* {{{ */
PHP_FUNCTION(beast_run_file)
{
    char *input_file;
    int input_len, offset;
    php_stream *input_stream;
    php_stream_statbuf stat_ssb;
    int fsize, bsize, msize, i;
    char *buff, *script, input[8], *compname;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "sl", 
	                             &input_file, &input_len, &offset) == FAILURE) {
		return;
	}

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
        RETURN_FALSE;
    }

    buff = (char *)malloc(msize + 4); /* alloc memory for cache */
    if (buff == NULL) {
        php_stream_close(input_stream);
        php_error_docref(NULL TSRMLS_CC, E_ERROR,
                                              "Unable alloc memory for buffer");
        RETURN_FALSE;
    }

    php_stream_seek(input_stream, offset, SEEK_SET);

    /* for closing php script environment */
    buff[0] = ' ';
    buff[1] = '?';
    buff[2] = '>';

    script = &buff[3];

    for (i = 0; i < bsize; i++) {
        (void)php_stream_read(input_stream, input, 8);
        DES_decipher(input, &(script[i * 8]), __authkey);
    }

    buff[msize+3] = '\0';

    compname = zend_make_compiled_string_description("PHPBEAST" TSRMLS_CC);

    if (zend_eval_string(buff, NULL, compname TSRMLS_CC) == FAILURE) {
        php_stream_close(input_stream);
        free(buff);
        php_error_docref(NULL TSRMLS_CC, E_ERROR,
                                                "Unable execute script string");
        RETURN_FALSE;
    }

    php_stream_close(input_stream);
    free(buff); /* free memory */

    RETURN_TRUE;
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

    for (i = 0; i < bsize; i++)
    {
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
/* }}} */


/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
