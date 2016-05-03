#ifndef __ALGO_OPS_STRUCT_H
#define __ALGO_OPS_STRUCT_H

typedef int beast_encrypt_op_t(char *inbuf, int inlen,
    char **outbuf, int *outlen);
typedef int beast_decrypt_op_t(char *inbuf, int inlen,
    char **outbuf, int *outlen);
typedef void beast_free_buf_t(void *buf);

struct beast_ops {
    char *name;
    beast_encrypt_op_t *encrypt;
    beast_decrypt_op_t *decrypt;
    beast_free_buf_t *free;
    struct beast_ops *next;
};

#endif
