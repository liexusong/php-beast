#ifndef BEAST_LOG_H
#define BEAST_LOG_H

typedef enum {
    beast_log_debug,  /* 0 */
    beast_log_notice, /* 1 */
    beast_log_error   /* 2 */
} beast_log_level;

void beast_write_log(beast_log_level level, const char *fmt, ...);

#endif
