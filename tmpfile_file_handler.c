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
    struct tmpfile_handler_ctx *ctx = self->ctx;

    ctx->fp = tmpfile();
    if (!ctx->fp) {
        return -1;
    }

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
    FILE *retval;

    retval = ctx->fp;
    ctx->fp = NULL;

    return retval;
}

int tmpfile_handler_get_fd(struct file_handler *self)
{
    return -1;
}

int tmpfile_handler_destroy(struct file_handler *self)
{
    struct tmpfile_handler_ctx *ctx = self->ctx;

    if (ctx->fp) {
        fclose(ctx->fp);
    }

    ctx->fp = NULL;

    return 0;
}

static struct tmpfile_handler_ctx _ctx = {
	NULL
};

struct file_handler tmpfile_handler = {
	"tmpfile",
	BEAST_FILE_HANDLER_FP,
	&_ctx,
	tmpfile_handler_check,
	tmpfile_handler_open,
	tmpfile_handler_write,
	tmpfile_handler_rewind,
	tmpfile_handler_get_fd,
	tmpfile_handler_get_fp,
	tmpfile_handler_destroy
};
