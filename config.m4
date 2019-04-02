dnl $Id$
dnl config.m4 for extension beast

dnl Comments in this file start with the string 'dnl'.
dnl Remove where necessary. This file will not work
dnl without editing.

dnl If your extension references something external, use with:

dnl PHP_ARG_WITH(beast, for beast support,
dnl Make sure that the comment is aligned:
dnl [  --with-beast             Include beast support])

dnl Otherwise use enable:

PHP_ARG_ENABLE(beast, whether to enable beast support,
dnl Make sure that the comment is aligned:
[  --enable-beast           Enable beast support])

PHP_ARG_ENABLE(beast-debug, whether to enable beast debug mode,
dnl Make sure that the comment is aligned:
[  --enable-beast-debug            Enable beast debug mode], no, no)

PHP_ARG_ENABLE(execute-normal-script, whether to enable execute normal PHP script mode,
dnl Make sure that the comment is aligned:
[  --enable-execute-normal-script  Enable execute normal PHP script], yes, yes)

if test "$PHP_BEAST" != "no"; then
  dnl Write more examples of tests here...

  if test "$PHP_BEAST_DEBUG" != "yes"; then
    AC_DEFINE(BEAST_DEBUG_MODE, 0, [ ])
  else
    AC_DEFINE(BEAST_DEBUG_MODE, 1, [ ])
  fi

  if test "$PHP_EXECUTE_NORMAL_SCRIPT" != "yes"; then
    AC_DEFINE(BEAST_EXECUTE_NORMAL_SCRIPT, 0, [ ])
  else
    AC_DEFINE(BEAST_EXECUTE_NORMAL_SCRIPT, 1, [ ])
  fi

  dnl # --with-beast -> check with-path
  dnl SEARCH_PATH="/usr/local /usr"     # you might want to change this
  dnl SEARCH_FOR="/include/beast.h"  # you most likely want to change this
  dnl if test -r $PHP_BEAST/$SEARCH_FOR; then # path given as parameter
  dnl   BEAST_DIR=$PHP_BEAST
  dnl else # search default path list
  dnl   AC_MSG_CHECKING([for beast files in default path])
  dnl   for i in $SEARCH_PATH ; do
  dnl     if test -r $i/$SEARCH_FOR; then
  dnl       BEAST_DIR=$i
  dnl       AC_MSG_RESULT(found in $i)
  dnl     fi
  dnl   done
  dnl fi
  dnl
  dnl if test -z "$BEAST_DIR"; then
  dnl   AC_MSG_RESULT([not found])
  dnl   AC_MSG_ERROR([Please reinstall the beast distribution])
  dnl fi

  dnl # --with-beast -> add include path
  dnl PHP_ADD_INCLUDE($BEAST_DIR/include)

  dnl # --with-beast -> check for lib and symbol presence
  dnl LIBNAME=beast # you may want to change this
  dnl LIBSYMBOL=beast # you most likely want to change this

  dnl PHP_CHECK_LIBRARY($LIBNAME,$LIBSYMBOL,
  dnl [
  dnl   PHP_ADD_LIBRARY_WITH_PATH($LIBNAME, $BEAST_DIR/lib, BEAST_SHARED_LIBADD)
  dnl   AC_DEFINE(HAVE_BEASTLIB,1,[ ])
  dnl ],[
  dnl   AC_MSG_ERROR([wrong beast lib version or lib not found])
  dnl ],[
  dnl   -L$BEAST_DIR/lib -lm -ldl
  dnl ])
  dnl
  dnl PHP_SUBST(BEAST_SHARED_LIBADD)

  PHP_NEW_EXTENSION(beast, beast.c aes_algo_handler.c des_algo_handler.c base64_algo_handler.c beast_mm.c spinlock.c cache.c beast_log.c global_algo_modules.c header.c networkcards.c tmpfile_file_handler.c pipe_file_handler.c file_handler_switch.c shm.c, $ext_shared)
fi
