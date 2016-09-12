#ifndef __BEAST_MODULE_H
#define __BEAST_MODULE_H

typedef int beast_encrypt_op_t(char *inbuf, int inlen,
    char **outbuf, int *outlen);
typedef int beast_decrypt_op_t(char *inbuf, int inlen,
    char **outbuf, int *outlen);
typedef void beast_free_buf_t(void *buf);

typedef enum {
  BEAST_ENCRYPT_TYPE_DES = 1,
  BEAST_ENCRYPT_TYPE_AES,
  BEAST_ENCRYPT_TYPE_BASE64,
  BEAST_ENCRYPT_TYPE_ERROR
} beast_encrypt_type_t;

struct beast_ops {
    char *name;
    beast_encrypt_op_t *encrypt;
    beast_decrypt_op_t *decrypt;
    beast_free_buf_t *free;
    struct beast_ops *next;
};

#endif
