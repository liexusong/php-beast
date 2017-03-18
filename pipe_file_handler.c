#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include "file_handler.h"

struct pipe_handler_ctx {
    int fd[2];
};

int set_nonblock(int fd)
{
    int flags;

    if ((flags = fcntl(fd, F_GETFL, 0)) == -1
        || fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1)
    {
        return -1;
    }
    return 0;
}

int pipe_handler_check()
{
    int fd[2];
    int max = 0;

    if (pipe(fd) != 0 || set_nonblock(fd[1]) != 0) {
        return -1;
    }

    for (;;) {
        if (write(fd[1], "\0", 1) != 1)
            break;
        max++;
    }

    close(fd[0]);
    close(fd[1]);

    return max;
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

    return 0;
}


struct file_handler pipe_handler = {
    .name    = "pipe",
    .check   = pipe_handler_check,
    .open    = pipe_handler_open,
    .write   = pipe_handler_write,
    .rewind  = pipe_handler_rewind,
    .get_fp  = pipe_handler_get_fp,
    .get_fd  = pipe_handler_get_fd,
    .destroy = pipe_handler_destroy
};
