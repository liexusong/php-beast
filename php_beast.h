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

#ifndef PHP_BEAST_H
#define PHP_BEAST_H

extern zend_module_entry beast_module_entry;
#define phpext_beast_ptr &beast_module_entry

#ifdef PHP_WIN32
#define PHP_BEAST_API __declspec(dllexport)
#else
#define PHP_BEAST_API
#endif

#ifdef ZTS
#include "TSRM.h"
#endif

PHP_MINIT_FUNCTION(beast);
PHP_MSHUTDOWN_FUNCTION(beast);
PHP_RINIT_FUNCTION(beast);
PHP_RSHUTDOWN_FUNCTION(beast);
PHP_MINFO_FUNCTION(beast);

PHP_FUNCTION(beast_encode_file);
PHP_FUNCTION(beast_avail_cache);
PHP_FUNCTION(beast_support_filesize);
PHP_FUNCTION(beast_file_expire);
PHP_FUNCTION(beast_clean_cache);

/*
  	Declare any global variables you may need between the BEGIN
	and END macros here:

ZEND_BEGIN_MODULE_GLOBALS(beast)
	long  global_value;
	char *global_string;
ZEND_END_MODULE_GLOBALS(beast)
*/

/* In every utility function you add that needs to use variables
   in php_beast_globals, call TSRMLS_FETCH(); after declaring other
   variables used by that function, or better yet, pass in TSRMLS_CC
   after the last function argument and declare your utility function
   with TSRMLS_DC after the last declared argument.  Always refer to
   the globals in your function as BEAST_G(variable).  You are
   encouraged to rename these macros something shorter, see
   examples in any other php module directory.
*/

#ifdef ZTS
#define BEAST_G(v) TSRMG(beast_globals_id, zend_beast_globals *, v)
#else
#define BEAST_G(v) (beast_globals.v)
#endif

#endif	/* PHP_BEAST_H */


/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker expandtab
 * vim<600: noet sw=4 ts=4
 */
