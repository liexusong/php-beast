/**
 * Base64 encrypt algorithms handler module for Beast
 * @author: liexusong
 */

#include <stdlib.h>
#include <string.h>
#include "beast_log.h"
#include "beast_module.h"

static const char base64_table[] =
{ 'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M',
  'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z',
  'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm',
  'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z',
  '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '+', '/', '\0'
};

static const char base64_pad = '=';

static const short base64_reverse_table[256] = {
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 62, -1, -1, -1, 63,
	52, 53, 54, 55, 56, 57, 58, 59, 60, 61, -1, -1, -1, -1, -1, -1,
	-1,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14,
	15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, -1, -1, -1, -1, -1,
	-1, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
	41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1
};

char *base64_encode(char *str, int length, int *ret_length)
{
	char *current = str;
	char *p;
	char *result;
	int alen;

	if ((length + 2) < 0
		|| ((length + 2) / 3) >= (1 << (sizeof(int) * 8 - 2)))
	{
		if (ret_length != NULL) {
			*ret_length = 0;
		}
		return NULL;
	}

	alen = ((length + 2) / 3) * 4 + 1;

	result = malloc(alen);
	if (!result) {
		beast_write_log(beast_log_error,
              "Out of memory when allocate `%d' size by encrypt(BASE64)", alen);
		return NULL;
	}

	p = result;

	while (length > 2) { /* keep going until we have less than 24 bits */
		*p++ = base64_table[current[0] >> 2];
		*p++ = base64_table[((current[0] & 0x03) << 4) + (current[1] >> 4)];
		*p++ = base64_table[((current[1] & 0x0f) << 2) + (current[2] >> 6)];
		*p++ = base64_table[current[2] & 0x3f];

		current += 3;
		length -= 3; /* we just handle 3 octets of data */
	}

	/* now deal with the tail end of things */
	if (length != 0) {
		*p++ = base64_table[current[0] >> 2];
		if (length > 1) {
			*p++ = base64_table[((current[0] & 0x03) << 4) + (current[1] >> 4)];
			*p++ = base64_table[(current[1] & 0x0f) << 2];
			*p++ = base64_pad;
		} else {
			*p++ = base64_table[(current[0] & 0x03) << 4];
			*p++ = base64_pad;
			*p++ = base64_pad;
		}
	}

	if (ret_length != NULL) {
		*ret_length = (int)(p - result);
	}

	*p = '\0';

	return result;
}


char *base64_decode(char *str, int length, int *ret_length)
{
	char *current = str;
	int ch, i = 0, j = 0, k;
	char *result;

	result = malloc(length + 1);
	if (result == NULL) {
		beast_write_log(beast_log_error,
          "Out of memory when allocate `%d' size by decrypt(BASE64)", length+1);
		return NULL;
	}

	/* run through the whole string, converting as we go */
	while ((ch = *current++) != '\0' && length-- > 0) {
		if (ch == base64_pad) break;

		ch = base64_reverse_table[ch];
		if (ch < 0) continue;

		switch(i % 4) {
		case 0:
			result[j] = ch << 2;
			break;
		case 1:
			result[j++] |= ch >> 4;
			result[j] = (ch & 0x0f) << 4;
			break;
		case 2:
			result[j++] |= ch >>2;
			result[j] = (ch & 0x03) << 6;
			break;
		case 3:
			result[j++] |= ch;
			break;
		}
		i++;
	}

	k = j;
	/* mop things up if we ended on a boundary */
	if (ch == base64_pad) {
		switch(i % 4) {
		case 1:
			free(result);
			return NULL;
		case 2:
			k++;
		case 3:
			result[k++] = 0;
		}
	}

	if(ret_length) {
		*ret_length = j;
	}

	result[j] = '\0';

	return result;
}


int base64_encrypt_handler(char *inbuf, int len,
	char **outbuf, int *outlen)
{
	char *result;
	int reslen;

	result = base64_encode(inbuf, len, &reslen);
	if (!result) {
		return -1;
	}

	*outbuf = result;
	*outlen = reslen;

	return 0;
}


int base64_decrypt_handler(char *inbuf, int len,
	char **outbuf, int *outlen)
{
	char *result;
	int reslen;

	result = base64_decode(inbuf, len, &reslen);
	if (!result) {
		return -1;
	}

	*outbuf = result;
	*outlen = reslen;

	return 0;
}


void base64_free_handler(void *ptr)
{
    if (ptr) {
        free(ptr);
    }
}


struct beast_ops base64_handler_ops = {
	"base64-algo",
	base64_encrypt_handler,
	base64_decrypt_handler,
	base64_free_handler,
	NULL
};
