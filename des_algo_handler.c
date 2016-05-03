/**
 * DES algorithms handler for php-beast
 * @author: liexusong
 */

#include <stdlib.h>
#include <string.h>
#include "ops_struct.h"
#include "des_algo.c"


static char key[8] = {
    0x01, 0x1f, 0x01, 0x1f,
    0x01, 0x0e, 0x01, 0x0e,
};


int des_encrypt_handler(char *inbuf, int len,
	char **outbuf, int *outlen)
{
    int blocks, i, fixcnt;
    char input[8], output[8];
    char *out;
    int retlen;

    if ((len % 8) == 0) {
        fixcnt = 0;
        blocks = len / 8;

    } else {
        fixcnt = len % 8;
        blocks = len / 8 + 1;
    }

    retlen = blocks * 8;

    out = malloc(retlen);
    if (!out) {
    	return -1;
    }

    for (i = 0; i < blocks; i++) {
        memset(input, 0, 8);

        /* The last block not enough 8 bytes, fix me */
        if (i + 1 == blocks && fixcnt > 0) {
            memcpy(input, &inbuf[i*8], fixcnt);
        } else {
            memcpy(input, &inbuf[i*8], 8);
        }

        DES_encipher(input, output, key);

        memcpy(&out[i * 8], output, 8);
    }

    *outbuf = out;
    *outlen = retlen;

    return 0;
}


int des_decrypt_handler(char *inbuf, int len,
	char **outbuf, int *outlen)
{
    int blocks, retlen, i;
    char *out;

    if (len % 8 == 0) {
        blocks = len / 8;
    } else {
        blocks = len / 8 + 1;
    }

    retlen = blocks * 8;

    out = malloc(retlen);
    if (!out) {
    	return -1;
    }

    for (i = 0; i < blocks; i++) {
        DES_decipher(&inbuf[i*8], &out[i*8], key);
    }

    *outbuf = out;
    *outlen = retlen;

    return 0;
}


void des_free_handler(void *ptr)
{
    if (ptr) {
        free(ptr);
    }
}

struct beast_ops des_handler_ops = {
	.name = "des-algo",
	.encrypt = des_encrypt_handler,
	.decrypt = des_decrypt_handler,
	.free = des_free_handler
};
