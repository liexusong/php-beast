#ifndef BEAST_LOG_H
#define BEAST_LOG_H

#ifdef PHP_WIN32
#include "win95nt.h"
#else
#include <unistd.h>
#endif

typedef enum {
    beast_log_debug,  /* 0 */
    beast_log_notice, /* 1 */
    beast_log_error   /* 2 */
} beast_log_level;

int beast_log_init(char *log_file, int level);
int beast_log_chown(uid_t uid, gid_t gid);
void beast_write_log(beast_log_level level, const char *fmt, ...);
void beast_log_destroy();

#endif
