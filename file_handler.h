#ifndef __FILE_HANDLER_H
#define __FILE_HANDLER_H

#include <stdio.h>

#define BEAST_FILE_HANDLER_FP 1
#define BEAST_FILE_HANDLER_FD 2

struct file_handler {
    char *name;
    int type;
    void *ctx;
    int (*check)();
    int (*open)(struct file_handler *self);
    int (*write)(struct file_handler *self, char *buf, int size);
    int (*rewind)(struct file_handler *self);
    int (*get_fd)(struct file_handler *self);
    FILE *(*get_fp)(struct file_handler *self);
    int (*destroy)(struct file_handler *self);
};

#endif
