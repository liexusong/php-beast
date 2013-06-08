/*****************************************************************************
*                                                                            *
*  ------------------------------- encrypt.h ------------------------------  *
*                                                                            *
*****************************************************************************/

#ifndef ENCRYPT_H
#define ENCRYPT_H

#include "zend.h"
#include "zend_API.h"

/*****************************************************************************
*                                                                            *
*  --------------------------- Public Interface ---------------------------  *
*                                                                            *
*****************************************************************************/

int encrypt_file(const char *inputfile, const char *outputfile, const char *key TSRMLS_DC);
int decrypt_file_return_buffer(const char *inputfile, const char *key, char **buf, int *len TSRMLS_DC);

#endif
