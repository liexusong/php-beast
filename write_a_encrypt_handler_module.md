加密模块编写教程
================

一，首先创建一个.c的文件。例如我们要编写一个使用base64加密的模块，可以创建一个名叫base64_algo_handler.c的文件。如何在文件添加如下代码：
<pre><code>
#include "beast_module.h"

int base64_encrypt_handler(char *inbuf, int len, char **outbuf, int *outlen)
{
    ...
}

int base64_decrypt_handler(char *inbuf, int len, char **outbuf, int *outlen)
{
    ...
}

void base64_free_handler(void *ptr)
{
    ...
}

struct beast_ops des_handler_ops = {
	.name = "base64-algo",
	.encrypt = base64_encrypt_handler,
	.decrypt = base64_decrypt_handler,
	.free = base64_free_handler,
};
</code></pre>