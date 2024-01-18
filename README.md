# naslite
# 介绍
- 这是一个开源的轻量级nas管理软件，包括http反向代理，socks5反向代理，静态web服务器，nas应用进程管理；
- 此项目来源于我个人的需求，Windows Server 2022系统，家里有公网IP，主要做家庭相册和看电影用，后端应用有FileBrowser,Jyellyfin,Gitea,SiYuan,Syncthing等（后端所有应用均为开源项目，非开源项目的应用基本上不考虑），但我担心这些后端应用会不会哪里有尚未暴露的漏洞，所以想统一NAS入口，让整个NAS只有一两个端口开放，因此做了http/https反向代理，所有后端应用全部本地化，只能通过反向代理才能访问，代理支持对后端应用的登录进行拦截，登录失败次数过多就锁定IP禁止访问，一定程度上避免对账号密码的暴力尝试，这对那些没有验证码也没有对登录失败进行安全处理的后端应用，有一定的保护作用；socks5反向代理，是用于一些其它服务的访问(如远程桌面)，配合SocksCap这个软件，就可以实现NAS上的所有应用都必须通过反向代理才能访问；根据入口的日志监控，出了问题也能大致锁定原因在哪里。

# 特性
- 完全开源
- 依赖：C++23 boost asio coroutine vue3
- 目前仅做了Windows系统下的编译和使用

# 功能模块
- http/https反向代理
- socks5反向代理
- 静态http/https web服务器
- nas应用进程管理
- 支持通过web进行远程管理

# 演示站点
- 自用NAS：[https://nav.nextvr.top:8888/](https://nav.nextvr.top:8888/)

# 所用服务
- 域名-阿里云DDNS SDK： [https://api.aliyun.com/api/Alidns/2015-01-09/UpdateDomainRecord?tab=DEMO&lang=CSHARP](https://api.aliyun.com/api/Alidns/2015-01-09/UpdateDomainRecord?tab=DEMO&lang=CSHARP)
- SSL证书(泛域名)：win-acme

# web远程管理(自适应电脑屏幕和手机端)

![](./doc/index-pc.png)

![](./doc/http_reverse_proxy-pc.png)

![](./doc/service_process_mgr-phone.png)

![](./doc/frontend_http_server-phone.png)
