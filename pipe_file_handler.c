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
    struct pipe_handler_ctx *ctx;

    ctx = malloc(sizeof(*ctx));
    if (!ctx) {
        return -1;
    }

    if (pipe(ctx->fd) == -1) {
        free(ctx);
        return -1;
    }

    self->type = BEAST_FILE_HANDLER_FD;
    self->ctx = ctx;

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
    int fd = ctx->fd[0];

    close(ctx->fd[1]);
    free(self->ctx);

    self->ctx = NULL;

    return fd;
}

int pipe_handler_destroy(struct file_handler *self)
{
    struct pipe_handler_ctx *ctx = self->ctx;

    if (!ctx) {
        return 0;
    }

    if (ctx->fd[0] != -1)
        close(ctx->fd[0]);
    if (ctx->fd[1] != -1)
        close(ctx->fd[1]);
    free(self->ctx);

    self->ctx = NULL;

    return 0;
}


struct file_handler pipe_handler = {
    .name    = "pipe",
    .ctx     = NULL,
    .check   = pipe_handler_check,
    .open    = pipe_handler_open,
    .write   = pipe_handler_write,
    .rewind  = pipe_handler_rewind,
    .get_fp  = pipe_handler_get_fp,
    .get_fd  = pipe_handler_get_fd,
    .destroy = pipe_handler_destroy
};
