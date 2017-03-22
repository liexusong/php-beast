
#ifndef PHP_WIN32

#include <stdlib.h>
#include <stdio.h>
#include "file_handler.h"

struct pipe_handler_ctx {
    int fd[2];
};

int pipe_handler_check()
{
    return 64 * 1024;
}

int pipe_handler_open(struct file_handler *self)
{
    struct pipe_handler_ctx *ctx = self->ctx;

    if (pipe(ctx->fd) == -1) {
        ctx->fd[0] = -1;
        ctx->fd[1] = -1;
        return -1;
    }

    return 0;
}

int pipe_handler_write(struct file_handler *self, char *buf, int size)
{
    struct pipe_handler_ctx *ctx = self->ctx;

    if (write(ctx->fd[1], buf, size) == size) {
        return 0;
    }

    return -1;
}

int pipe_handler_rewind(struct file_handler *self)
{
    return 0;
}

FILE *pipe_handler_get_fp(struct file_handler *self)
{
    return NULL;
}

int pipe_handler_get_fd(struct file_handler *self)
{
    struct pipe_handler_ctx *ctx = self->ctx;
    int retval;

    retval = ctx->fd[0];

    close(ctx->fd[1]);  /* Closed write pipe */

    ctx->fd[0] = -1;
    ctx->fd[1] = -1;

    return retval;
}

int pipe_handler_destroy(struct file_handler *self)
{
    struct pipe_handler_ctx *ctx = self->ctx;

    if (ctx->fd[0] != -1)
        close(ctx->fd[0]);
    if (ctx->fd[1] != -1)
        close(ctx->fd[1]);

    ctx->fd[0] = -1;
    ctx->fd[1] = -1;

    return 0;
}

static struct pipe_handler_ctx _ctx = {
    {-1, -1}
};

struct file_handler pipe_handler = {
    "pipe",
    BEAST_FILE_HANDLER_FD,
    &_ctx,
    pipe_handler_check,
    pipe_handler_open,
    pipe_handler_write,
    pipe_handler_rewind,
    pipe_handler_get_fd,
    pipe_handler_get_fp,
    pipe_handler_destroy
};

#endif
