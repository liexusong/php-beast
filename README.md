<pre><code>
  _____  _    _ _____    ____  ______           _____ _______ 
 |  __ \| |  | |  __ \  |  _ \|  ____|   /\    / ____|__   __|
 | |__) | |__| | |__) | | |_)/| |__     /  \  | (___    | |   
 |  ___/|  __  |  ___/  |  _ ||  __|   / /\ \  \___ \   | |   
 | |    | |  | | |      | |_)\| |____ / ____ \ ____) |  | |   
 |_|    |_|  |_|_|      |____/|______/_/    \_\_____/   |_|   
 
 此模块可以用于商业, 使用DES加密算法加密, 版权归原作者.
</code></pre>

<h3>使用方法:</h3>
<pre>
修改key.c文件的__authkey加密key, 编译安装, 接着使用beast_encode_file()函数进行加密, 加密后的文件beast模块能自动识别.<br />
使用案例: <a href="http://www.beastcoder.com">http://www.beastcoder.com</a><br />
通过网址: <a href="http://www.beastcoder.com/info.php">http://www.beastcoder.com/info.php</a> 可以看到本扩展的信息.
</pre>


编译安装如下:
-------------
<pre><code>
$ wget https://github.com/liexusong/php-beast/archive/master.zip
$ unzip master.zip
$ cd php-beast-master
$ phpize
$ ./configure
$ sudo make && make install
</code></pre>

编译好之后修改php.ini配置文件, 加入配置项: extension=beast.so, 重启php-fpm

<pre><code>
  温馨提示: 1) 缓存设置越大, 效率越高
            2) 可以修改key.c文件中的加密key
</code></pre>

<p>使用php-beast的性能：<br/>
<img src="http://git.oschina.net/liexusong/php-beast/raw/master/images/beast1.png?dir=0&filepath=images/beast1.png&oid=645b87003dada2eac4f1a9fcfd353aa0423f5711&sha=7ec2a0ddc7780b2bab538d9f49d8b262f1bc37b7" /></p>

<p>不使用php-beast的性能：<br/>
<img src="http://git.oschina.net/liexusong/php-beast/raw/master/images/beast2.png?dir=0&filepath=images/beast2.png&oid=3f07cff6dca34b22d8933ab0ea1740a0e4f37e34&sha=7ec2a0ddc7780b2bab538d9f49d8b262f1bc37b7" /></p>

配置项:
<pre><code>
 beast.cache_size = size
 beast.log_file = "path_to_log"
 beast.enable = On
</code></pre>

通过测试环境:
<pre><code>
 Nginx + Fastcgi + (PHP-5.3.x/PHP-5.2.x)
</code></pre>


注意
----
如果出现502错误，一般是由于GCC版本太低导致，请先升级GCC再安装本模块。


TODO:
-----
* 增加opcode缓存

注意:
-----
win32文件夹中是win版本的加密模块, 但是因为没有缓存, 所以效率比较低, 建议使用时不要大量加密文件.
因为本人不太熟识win平台, 所以希望能有win平台的编程高手与本人一起完成win的版本. 有意请联系QQ: 280259971<br/><br/>


------------------------------
作者: 列旭松(280259971@qq.com) 专业定制PHP扩展、Nginx模块。

我的著作: http://book.jd.com/11123177.html<br/>
<b>提供安装配置服务和定制版本, 请联系QQ: 280259971</b><br/>
捐赠本项目: <img width="250" src="https://tfsimg.alipay.com/images/mobilecodec/T16NxhXe8lXXXXXXXX" />
------------------------------
