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

if test "$PHP_BEAST" != "no"; then
  dnl Write more examples of tests here...

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

  PHP_NEW_EXTENSION(beast, beast.c encrypt.c bit.c, $ext_shared)
fi
