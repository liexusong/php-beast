加密模块编写教程
================
<b>一，首先创建一个.c的文件。例如我们要编写一个使用base64加密的模块，可以创建一个名叫base64_algo_handler.c的文件。然后在文件添加如下代码：</b>
<pre>
<code lang="c">
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

struct beast_ops base64_handler_ops = {
    .name = "base64-algo",
	.encrypt = base64_encrypt_handler,
	.decrypt = base64_decrypt_handler,
	.free = base64_free_handler,
};
</code>
</pre>

<b>模块必须实现3个方法，分别是：encrypt、decrypt、free方法。</b>

+ 1) encrypt方法负责把inbuf字符串加密，然后通过outbuf输出给beast。
+ 2) decrypt方法负责把加密数据inbuf解密，然后通过outbuf输出给beast。
+ 3) free方法负责释放encrypt和decrypt方法生成的数据。

<b>二，写好我们的加密模块后，需要在global_algo_modules.c添加我们模块的信息。代码如下：</b>
<pre><code lang="c">
#include &lt;stdlib.h&gt;
#include "beast_module.h"

extern struct beast_ops des_handler_ops;

struct beast_ops *ops_handler_list[] = {
    &des_handler_ops,
    &base64_handler_ops, /* 这里是我们的模块信息 */
	NULL,
};
</code></pre>

<b>三，修改config.m4文件，修改倒数第二行，如下代码：</b>
<pre><code>
PHP_NEW_EXTENSION(beast, beast.c des_algo_handler.c beast_mm.c spinlock.c cache.c beast_log.c global_algo_modules.c <b>base64_algo_handler.c</b>, $ext_shared)
</code></pre>

加粗的代码是我们添加的，这里加入的是我们模块的文件名。<br/>
现在大功告成了，可以编译试下。如果要使用我们刚编写的加密算法来加密php文件，可以修改php.ini文件的配置项，如下：
<pre><code>
beast.encrypt_handler = "base64-algo"
</code></pre>
名字就是我们模块的name。