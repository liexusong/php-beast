#include <stdlib.h>
#include <stdio.h>
#include "file_handler.h"

struct tmpfile_handler_ctx {
    FILE *fp;
};

int tmpfile_handler_check()
{
    return 0;
}

int tmpfile_handler_open(struct file_handler *self)
{
    struct tmpfile_handler_ctx *ctx;

    ctx = malloc(sizeof(*ctx));
    if (!ctx) {
        return -1;
    }

    ctx->fp = tmpfile();
    if (!ctx->fp) {
        free(ctx);
        return -1;
    }

    self->type = BEAST_FILE_HANDLER_FP;
    self->ctx = ctx;

    return 0;
}

int tmpfile_handler_write(struct file_handler *self, char *buf, int size)
{
    struct tmpfile_handler_ctx *ctx = self->ctx;

    if (fwrite(buf, 1, size, ctx->fp) == size) {
        return 0;
    }

    return -1;
}

int tmpfile_handler_rewind(struct file_handler *self)
{
    struct tmpfile_handler_ctx *ctx = self->ctx;

    rewind(ctx->fp);

    return 0;
}

FILE *tmpfile_handler_get_fp(struct file_handler *self)
{
    struct tmpfile_handler_ctx *ctx = self->ctx;
    FILE *fp = ctx->fp;

    free(self->ctx);

    return fp;
}

int tmpfile_handler_get_fd(struct file_handler *self)
{
    return -1;
}

int tmpfile_handler_destroy(struct file_handler *self)
{
    struct tmpfile_handler_ctx *ctx = self->ctx;

    if (!ctx) {
        return 0;
    }

    if (ctx->fp)
        fclose(ctx->fp);
    free(self->ctx);

    return 0;
}


struct file_handler tmpfile_handler = {
    .name    = "tmpfile",
    .check   = tmpfile_handler_check,
    .open    = tmpfile_handler_open,
    .write   = tmpfile_handler_write,
    .rewind  = tmpfile_handler_rewind,
    .get_fp  = tmpfile_handler_get_fp,
    .get_fd  = tmpfile_handler_get_fd,
    .destroy = tmpfile_handler_destroy
};
