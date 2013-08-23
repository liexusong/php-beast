#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

#include "beast_log.h"

extern char *beast_log_file;

void beast_write_log(beast_log_level level, const char *fmt, ...)
{
    char *headers[] = {"{debug}", "{notice}", "{error}"};
    va_list ap;
    FILE *fp;
    char buf[64];
    time_t now;

    if (level > beast_log_error ||
        level < beast_log_debug) {
        return;
    }

    fp = fopen(beast_log_file, "w+");
    if (!fp) {
        return;
    }

    va_start(ap, fmt);

    now = time(NULL);
    strftime(buf, 64, "[%d %b %H:%M:%S]", gmtime(&now));
    fprintf(fp, "%s %s ", buf, headers[level]);
    vfprintf(fp, fmt, ap);
    fprintf(fp, "\n");
    fflush(fp);
    va_end(ap);

    fclose(fp);
    return;
}
