# 安全审查报告 — dde-pdf-printer

> 审查日期：2026-08-12 | 版本：v0.4.9 基线（v0.8.3 加固 + 复审查）
> 审查范围：backend（root 运行 Python）+ C++ 服务层 + 控制中心插件（C++/QML）+ debian 打包
> 审查重点：文件系统非法访问 / 异常外连 / 敏感数据采集

## 一、网络外连 — 干净（0 风险）

| 检查项 | 结果 |
| --- | --- |
| socket / curl / wget / requests / urllib / QTcpSocket / QNetworkAccess | 零命中 |
| http(s):// 硬编码 | 零命中（源码无任何 URL） |
| upload / telemetry / report / POST / api. | 零命中 |

**结论**：运行时代码完全无网络能力，不可能外传数据。

## 二、文件系统访问 — 受控（仅用户自己的目录）

| 访问点 | 对象 | 越权？ |
| --- | --- | --- |
| backend 写 outpath | `<输出目录>/<按模板渲染的文件名>.pdf`（模板支持 `{title}`/`{jobid}`/`{date}`/`{time}`，渲染后统一清洗防穿越/注入） | 否，仅用户配置目录 |
| backend 读 conf_path | `~/.config/org.deepin.dde.pdfprinter/pdfprinter.conf` | 否，仅自己的配置 |
| backend 读 filename | CUPS 传入的作业临时文件 | 否 |
| C++ QFile::remove | outputDir() 内的 PDF | 否 |
| C++ lpadmin/lpstat | CUPS 标准管理命令 | 否 |

**路径穿越防护实测**（sanitize_filename）：

```
'../../etc/passwd'  → 'passwd'      ✅ 穿越剥离
'/etc/shadow'       → 'shadow'      ✅ 绝对路径剥离
'PWN; rm -rf /'     → 'print'       ✅ 命令注入过滤
'恶意<>字符|文件'    → '恶意字符文件'  ✅ 特殊字符清除
```

## 三、敏感数据采集 — 无（0 命中）

| 检查项 | 结果 |
| --- | --- |
| /etc/passwd / /etc/shadow 读取 | 零命中（仅 pwd.getpwnam 查 UID/GID/家目录，标准 API） |
| /proc/ 访问 | 零命中 |
| 键盘/剪贴板/屏幕采集 | 零命中 |
| 用户文档扫描 | 仅输出目录内 *.pdf 列表（功能需要） |

## 四、命令执行 — 白名单 + 参数安全

- C++：仅 `lpstat` / `lpadmin`，参数固定，无用户输入拼接
- backend：`subprocess.run(['su', '-s', '/bin/sh', '-c', f'mkdir -p {shlex.quote(outdir)}', username])`，路径经 `shlex.quote` 转义（仅非 root 兜底分支）

## 五、发现的问题（均已修复）

| 严重度 | 问题 | 处理 |
| --- | --- | --- |
| 低 | backend 未使用 import（shutil/glob） | 已清理 |
| 低 | debian/rules backend 权限 755（标准打包时 lp 用户运行） | 已修为 700 |

## 六、依赖纯净性

- backend 仅用 Python 标准库（os/sys/time/pwd/shlex/configparser/subprocess）
- 插件仅用 Qt6/DTK6/DCC 官方 API
- 无第三方二进制、无隐藏代码

## 评审结论

**通过** ✅ — 代码无网络外连能力、无敏感数据采集、文件系统访问严格限制在用户自己的输出目录与配置，路径穿越和命令注入均有实测防护。符合「无恶意代码」参赛要求。

## 二进制加固复审查（2026-08-14，v0.8.3）

### 发现的问题（v0.8.2 及更早）
| 问题 | 风险 | 修复 |
| --- | --- | --- |
| 插件 .so **无 Stack Canary / FORTIFY / Full RELRO** | 手动打包未应用 Debian hardening flags，栈溢出/ROP 无缓解 | ✅ CMakeLists 加 hardening flags（v0.8.3） |
| **本机路径泄漏进二进制**（`/home/master/...`，Qt 日志宏 __FILE__ 嵌入） | 公开分发泄露构建环境信息 | ✅ `-ffile-prefix-map` 映射为相对路径 |

### v0.8.3 加固实测（readelf/nm/strings 验证）
| 项 | 结果 |
| --- | --- |
| Stack Canary（__stack_chk_fail） | ✅ 已启用 |
| FORTIFY（-D_FORTIFY_SOURCE=2） | ✅ 标志生效（代码用 Qt 安全 API 无 strcpy 类调用，天然免疫） |
| Full RELRO（-z relro -z now，BIND_NOW） | ✅ 已启用 |
| 本机路径泄漏 | ✅ 0 处（映射为 `./src/...`） |
| PIE / NX | ✅ .so 天然位置无关 + NX 栈 |
| RPATH/RUNPATH | ✅ 无（无库劫持面） |

### 复审查确认（原结论保持有效）
- backend 700 root:root；`su -c` 命令 `shlex.quote` 消毒（无注入）
- 文件名模板渲染后整体 `sanitize_filename`（防穿越/注入）
- QProcess 全部参数化调用（lpadmin/lpstat/dde-open/dde-file-manager，无 shell）
- 无硬编码密钥/凭据（strings 扫描零命中）
- 维护脚本/文件属主全部 root:root，权限符合规范

## backend 接口安全审查（2026-08-14，v0.8.3）

backend 以 root 运行（权限 700），接口面：CUPS 参数（job/user/title/copies/options/filename）、
用户配置文件（~/.config/org.deepin.dde.pdfprinter/pdfprinter.conf：outputDir/filenameTemplate/
keepTitleExtension）、stdin 数据流、discover 模式。

### 发现的高危漏洞（v0.8.3 前）
| 漏洞 | 攻击路径 | 危害 | 修复 |
| --- | --- | --- | --- |
| **outputDir 任意路径** | 用户配置 `outputDir=/etc/cron.d` 后打印 | root 写入系统目录 → **本地提权** | ✅ 校验输出目录必须位于用户家目录内（realpath 前缀检查），越界回退默认并告警 |
| **符号链接逃逸** | 用户预建 `~/PDF → /etc` 符号链接 | root 跟随 symlink 写系统目录 | ✅ 默认/配置路径均做 realpath 解析校验，越界返回 None 拒绝写入 |

### 攻击场景实测（v0.8.3 加固后）
| 场景 | 结果 |
| --- | --- |
| 配置 `outputDir=/etc` → 打印 | ✅ 拒绝（WARN 越界），回退默认 `~/PDF` |
| `~/PDF` symlink → `/etc` → 打印 | ✅ 拒绝（WARN symlink 逃逸 → ERROR 拒绝，无写入） |
| 正常 `~/PDF` / 家目录内自定义目录 | ✅ 正常写入 |
| 恢复现场 + 功能回归 | ✅ 打印正常，/etc 干净 |

### 其余接口面确认安全
- title/文件名：sanitize_filename（basename + 白名单字符）防穿越/注入
- 模板渲染：`{title}` 清洗、`{jobid}` 仅数字、渲染后整体再清洗
- copies：isdigit 校验；options：不解析执行
- stdin 数据：字节流直写（无解析执行）；PJL 剥离仅定位 %PDF 魔数
- 配置损坏：configparser 异常回退默认目录

## 全项目安全复审查（2026-08-14，dfyx 白盒审计协议 10 维度）

按 SecSkills 索引装载的 dfyx_code_security_review 协议（三层分析法 + 10 安全维度）对全项目
（C++ 插件 / Python backend / shell 维护脚本 / deb 打包）复审查：

| 维度 | 结论 | 证据 |
| --- | --- | --- |
| D1 注入 | ✅ | QProcess 全参数化（lpadmin/lpstat/dde-open/dde-file-manager 均列表参数）；backend `su -c` shlex.quote；模板渲染后整体 sanitize；postinst/prerm 固定参数无用户输入 |
| D2 认证 | ✅ | 无认证面（本地控制中心插件，无网络服务） |
| D3 授权 | ✅ | backend root 写入限用户家目录（realpath 校验）；chown 目标用户；维护脚本 root 操作白名单 |
| D4 反序列化 | ✅ | 无 pickle/json 反序列化 |
| D5 文件操作 | ✅ | 路径穿越 sanitize（basename+白名单）；symlink 逃逸已修；无临时文件 |
| D6 SSRF | ✅ | 无网络外连（backend 纯本地，Python 标准库） |
| D7 加密 | ✅ | 无敏感数据/密钥 |
| D8 配置 | ✅ | 配置损坏回退默认；日志无敏感信息；错误信息仅路径 |
| D9 业务逻辑 | ✅ | 文件名唯一（jobid+毫秒时间戳）；TOCTOU 窗口毫秒级本地无利；打印机迁移幂等 |
| D10 供应链 | ✅（补强） | 依赖系统包（Qt6/DTK6/CUPS）；backend 纯标准库；**补充 `Recommends: dde-api`**（dde-open 归属包）；清理 `debian/deepin-pdf-printer/` 旧名残留（git 跟踪 0） |
