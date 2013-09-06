#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include "beast_log.h"

static FILE *beast_log_fp = NULL;


int beast_log_init(char *log_file)
{
    if (!log_file || strlen(log_file) == 0) {
        return 0;
    }

    beast_log_fp = fopen(log_file, "a+");
    if (!beast_log_fp)
        return -1;
    return 0;
}


void beast_write_log(beast_log_level level, const char *fmt, ...)
{
    char *headers[] = {"[debug]", "[notice]", "[error]"};
    va_list ap;

    if (beast_log_fp == NULL || level > beast_log_error ||
        level < beast_log_debug)
    {
        return;
    }

    va_start(ap, fmt);

    fprintf(beast_log_fp, "%s ", headers[level]);
    vfprintf(beast_log_fp, fmt, ap);
    fprintf(beast_log_fp, "\n");
    fflush(beast_log_fp);

    va_end(ap);
    return;
}

void beast_log_destroy()
{
    if (beast_log_fp) {
        fclose(beast_log_fp);
        beast_log_fp = NULL;
    }
}
