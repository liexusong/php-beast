<pre><code>
  _____  _    _ _____    ____  ______           _____ _______
 |  __ \| |  | |  __ \  |  _ \|  ____|   /\    / ____|__   __|
 | |__) | |__| | |__) | | |_)/| |__     /  \  | (___    | |
 |  ___/|  __  |  ___/  |  _ ||  __|   / /\ \  \___ \   | |
 | |    | |  | | |      | |_)\| |____ / ____ \ ____) |  | |
 |_|    |_|  |_|_|      |____/|______/_/    \_\_____/   |_|
</code></pre>

<pre>
此模块可以用于商业用途, 版权归原作者.<br/>
QQ交流群：239243332
</pre>

<b>php-beast可以自定义加密模块，加密模块编写教程: <a href="https://github.com/liexusong/php-beast/wiki/%E5%8A%A0%E5%AF%86%E6%A8%A1%E5%9D%97%E7%BC%96%E5%86%99%E6%95%99%E7%A8%8B">点击</a></b>

<h3>编译安装如下:</h3>
<pre><code>
$ wget https://github.com/liexusong/php-beast/archive/master.zip
$ unzip master.zip
$ cd php-beast-master
$ phpize
$ ./configure
$ sudo make && make install

编译好之后修改php.ini配置文件, 加入配置项: extension=beast.so, 重启php-fpm
</code></pre>

<pre>温馨提示: 可以设置较大的缓存提高效率</pre>

<p><b>使用php-beast的性能：</b><br/><br/>
<img src="http://git.oschina.net/liexusong/php-beast/raw/master/images/beast1.png?dir=0&filepath=images/beast1.png&oid=645b87003dada2eac4f1a9fcfd353aa0423f5711&sha=7ec2a0ddc7780b2bab538d9f49d8b262f1bc37b7" /></p>

<p><b>不使用php-beast的性能：</b><br/><br/>
<img src="http://git.oschina.net/liexusong/php-beast/raw/master/images/beast2.png?dir=0&filepath=images/beast2.png&oid=3f07cff6dca34b22d8933ab0ea1740a0e4f37e34&sha=7ec2a0ddc7780b2bab538d9f49d8b262f1bc37b7" /></p>

配置项:
<pre><code>
 beast.cache_size = size
 beast.log_file = "path_to_log"
 beast.enable = On
 beast.encrypt_handler = "des-algo"
</code></pre>

支持的模块有：
<pre>
 1. AES
 2. DES
 3. Base64
</pre>

通过测试环境:
<pre><code>
 Nginx + Fastcgi + (PHP-5.2.x ~ PHP-5.6.x)
</code></pre>

<h3>注意</h3>
如果出现502错误，一般是由于GCC版本太低导致，请先升级GCC再安装本模块。

------------------------------

## 怎么加密项目
安装完 `php-beast` 后可以使用 `tools` 目录下的 `encode_files.php` 来加密你的项目。使用 `encode_files.php` 之前先修改 `tools` 目录下的 `configure.ini` 文件，如下：
```ini
; source path
src_path = ""

; destination path
dst_path = ""

; expire time
expire = ""
```
`src_path` 是要加密项目的路径，`dst_path` 是保存加密后项目的路径，`expire` 是设置项目可使用的时间 (`expire` 的格式是：`YYYY-mm-dd HH:ii:ss`)。
修改完 `configure.ini` 文件后就可以使用命令 `php encode_files.php` 开始加密项目。

------------------------------

## 制定自己的php-beast

`php-beast` 有多个地方可以定制的，以下一一列出：

*1.* 使用 `header.c` 文件可以修改 `php-beast` 加密后的文件头结构，这样网上的解密软件就不能认识我们的加密文件，就不能进行解密，增加加密的安全性。

*2.* `php-beast` 提供只能在指定的机器上运行的功能。要使用此功能可以在 `networkcards.c` 文件添加能够运行机器的网卡号，例如：
```c
char *allow_networkcards[] = {
	"fa:16:3e:08:88:01",
    NULL,
};
```
这样设置之后，`php-beast` 扩展就只能在 `fa:16:3e:08:88:01` 这台机器上运行。另外要注意的是，由于有些机器网卡名可能不一样，所以如果你的网卡名不是 `eth0` 的话，可以在 `php.ini` 中添加配置项： `beast.networkcard = "xxx"` 其中 `xxx` 就是你的网卡名。

*3.* 使用 `php-beast` 时最好不要使用默认的加密key，因为扩展是开源的，如果使用默认加密key的话，很容易被人发现。所以最好编译的时候修改加密的key，`aes模块` 可以在 `aes_algo_handler.c` 文件修改，而 `des模块` 可以在 `des_algo_handler.c` 文件修改。

------------------------------

作者: liexusong(280259971@qq.com)。

<b>my book:《<a href="http://book.jd.com/11123177.html">PHP核心技术与最佳实践</a>》</b>此书有详细的PHP扩展编写教程<br/>
## 如果本项目能够帮到你的话请支持一下，你的支持是我们最大的动力(*^__^*):
<p>
支付宝：<br />
<img width="250" src="https://tfsimg.alipay.com/images/mobilecodec/T16NxhXe8lXXXXXXXX" /></p>
<p>
微信：<br />
<img width="250" src="http://git.oschina.net/liexusong/php-beast/raw/master/images/pay.jpg?dir=0&filepath=images%2Fpay.jpg&oid=324fa98d10ed5db5a1ac5e765ce12db5d65cebd5&sha=20e9e714be2695829883a4055815a94d753545ec" />
</p>

------------------------------
