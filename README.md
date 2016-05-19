<pre><code>
  _____  _    _ _____    ____  ______           _____ _______
 |  __ \| |  | |  __ \  |  _ \|  ____|   /\    / ____|__   __|
 | |__) | |__| | |__) | | |_)/| |__     /  \  | (___    | |
 |  ___/|  __  |  ___/  |  _ ||  __|   / /\ \  \___ \   | |
 | |    | |  | | |      | |_)\| |____ / ____ \ ____) |  | |
 |_|    |_|  |_|_|      |____/|______/_/    \_\_____/   |_|
</code></pre>

```
此模块可以用于商业用途, 版权归原作者.
QQ交流群：239243332
```

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
作者: liexusong(280259971@qq.com)。

<b>my book:</b> http://book.jd.com/11123177.html<br/>
捐赠本项目: <img width="250" src="https://tfsimg.alipay.com/images/mobilecodec/T16NxhXe8lXXXXXXXX" />
------------------------------
