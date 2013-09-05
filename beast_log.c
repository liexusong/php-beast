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
    time_t now;
    char buf[64];

    if (level > beast_log_error ||
        level < beast_log_debug) {
        return;
    }

    fp = fopen(beast_log_file, "a+");
    if (!fp) {
        return;
    }

    now = time(NULL);
    strftime(buf, sizeof(buf), "%d %b %H:%M:%S", localtime(&now));

    fprintf(fp, "[%s] %s ", buf, headers[level]);

    va_start(ap, fmt);

    fprintf(fp, "%s ", );
    vfprintf(fp, fmt, ap);
    fprintf(fp, "\n");
    fflush(fp);

    va_end(ap);

    fclose(fp);
    return;
}
