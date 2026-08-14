# 独立安全审计报告（codex 子 Agent）

## 一、审计概述

- **审计时间**：2026-08-19
- **审计方法**：dfyx 三层分析法（面/线/点）+ D1~D10 十维安全审计
- **审计范围**：/home/master/Projects/deepin-pdf-printer
  - Python CUPS backend（backend/ddepdf）
  - C++ 服务层（src/service/*）
  - DDE 控制中心插件（src/plugin/operation/*、src/plugin/qml/*）
  - 打包与维护脚本（ci/package-deb.sh、debian/*.postinst/postrm/prerm）
  - 辅助脚本（scripts/*.py）
- **审计约束**：只读分析，未修改任何文件，未执行安装/打印/构建

---

## 二、项目架构与攻击面

### 2.1 架构图

```
打印对话框 → CUPS → backend/ddepdf (Python, root, 700) → 用户家目录/PDF
                                                          ↑
控制中心 ← src/plugin (C++/QML) → src/service (静态库) ← 同一配置文件
```

### 2.2 关键信任边界

| 边界 | 说明 |
|------|------|
| backend | 以 root 运行（debian 安装 700），接收 CUPS 传入的 job/user/title/filename/stdin |
| 插件/服务 | 以普通用户身份运行，读写用户配置与输出目录 |
| 配置文件 | `~/.config/org.deepin.dde.pdfprinter/pdfprinter.conf`（INI 格式） |

### 2.3 用户可控输入面

1. **CUPS 参数**：job、user、title、filename
2. **配置文件**：outputDir、filenameTemplate、keepTitleExtension
3. **输出目录**：目录内容可被用户提前构造（如符号链接）
4. **打印数据流**：stdin / 临时文件中的 PDF/PJL 数据
5. **控制中心 UI 操作**：选择目录、修改模板、删除/打开文件

---

## 三、按 D1~D10 逐维度审计结论

### D1 注入（命令注入 / 模板注入）

#### 结论
**未发现可利用注入漏洞**，但有一处低危代码风格点需关注。

#### 证据
- **QProcess 参数化调用**：所有 `QProcess::start` / `QProcess::startDetached` 均使用列表参数，无 shell 拼接。
  - `src/service/printermanager.cpp:52-57` lpstat
  - `src/service/printermanager.cpp:107-110` lpadmin
  - `src/service/printermanager.cpp:125` dde-file-manager
  - `src/service/printermanager.cpp:153` dde-open
- **backend 命令执行**：`backend/ddepdf:108-109` 在 PermissionError 兜底分支使用 `subprocess.run(['su', ...], f'mkdir -p {shlex.quote(outdir)}', ...)`，路径经 `shlex.quote` 转义，无命令注入。
- **文件名模板**：`render_filename()` 对 `{title}` 和整体结果调用 `sanitize_filename()`，限制为 `os.path.basename` + 白名单字符（`isalnum` 和 `._- `），无法引入路径分隔符或 shell 元字符。
- **CUPS title**：经 `sanitize_filename` 后进入文件名，不会穿越目录。
- **低危**：`backend/ddepdf:108-109` 的 `su -c` 虽然 quote 了 outdir，但该分支属于非 root 运行兜底，触发面极小。建议作为纵深防御点保留。

---

### D2 认证

#### 结论
**该维度不适用 / 安全**。

#### 证据
项目无网络服务、无 Token/Session/JWT、无认证中间件。控制中心插件在已登录用户会话中运行，依赖 DDE 会话自身的认证边界。backend 由 CUPS 调用，受 CUPS 权限模型保护。

---

### D3 授权（权限边界 / 提权）

#### 结论
**核心授权控制已正确实现，未发现本地提权漏洞**。

#### 证据
- **输出目录越界防护**：`backend/ddepdf:74-96` 使用 `os.path.realpath()` 解析配置目录，并校验其必须位于用户家目录下，否则回退默认目录或拒绝写入。
  - 检查点 1：配置读取后（第 69-79 行）
  - 检查点 2：默认/配置目录最终写入前（第 86-96 行）
- **符号链接逃逸防护**：`realpath` 会跟随符号链接，越界则回退/拒绝。
- **文件属主归还**：`backend/ddepdf:243-247` 将输出文件 `chown` 给目标用户。
- **backend 权限**：debian 安装 700，CUPS 以 root 执行，避免 lp 用户无法读取用户 700 配置的问题。
- **插件侧文件操作**：`PrinterManager::deletePdfFile()` / `openPdfFile()` 仅作用于 `outputDir()` 内的文件，文件名来自 `listPdfFiles()` 的目录枚举，不直接接受用户输入路径。

---

### D4 反序列化

#### 结论
**安全，无反序列化风险**。

#### 证据
- 未使用 `pickle`、`marshal`、`json.loads` 处理不可信数据。
- C++ 侧未使用 `QDataStream` 反序列化外部数据。
- backend 读取的是 `configparser` INI 配置，无反序列化语义。

---

### D5 文件操作（路径穿越 / 符号链接 / 临时文件）

#### 结论
**未发现可利用路径穿越或符号链接逃逸**。

#### 证据
- **路径穿越**：`sanitize_filename()` 取 `os.path.basename` 并过滤白名单字符，无法穿越。
- **符号链接**：`get_output_dir()` 使用 `os.path.realpath()` 解析后校验前缀。
- **临时文件**：未创建临时文件；输出文件路径由 `os.path.join(outdir, f'{base}{ts_ms:03d}.pdf')` 构造，无 mktemp 类竞争。
- **输出目录 chown**：创建后 `chown` 给用户。
- **插件侧目录选择**：通过 `com.deepin.filemanager.filedialog` D-Bus 服务选择，结果由官方对话框保证为合法本地目录。

---

### D6 SSRF

#### 结论
**安全，无 SSRF 面**。

#### 证据
- backend 与插件均无网络请求能力（无 socket/curl/urllib/QNetworkAccessManager）。
- `scripts/collect_forum_posts.py` 是开发/调研辅助脚本，包含网络请求，但不属于运行/分发产物，不在攻击面内。

---

### D7 加密 / 密钥

#### 结论
**安全，无加密/密钥管理问题**。

#### 证据
- 未使用任何加密算法、密钥、证书。
- `strings` / `nm` 扫描未命中硬编码密钥、token、密码。

---

### D8 配置（错误暴露 / 敏感信息）

#### 结论
**未发现敏感信息泄露或配置滥用**。

#### 证据
- 配置文件仅含 outputDir、filenameTemplate、keepTitleExtension、autoOpen。
- 日志文件（`~/.cache/org.deepin.dde.pdfprinter/pdfprinter.log`）记录功能调用与结果，不含密码、token 等敏感信息。
- backend 报错信息仅包含路径与状态，不泄露系统结构。
- 二进制已启用 `-ffile-prefix-map` 避免构建路径泄露（已在 CMakeLists.txt 中配置）。
- **低危提示**：日志文件为当前用户可读写，属于预期行为，无跨用户泄露风险。

---

### D9 业务逻辑（竞态 / TOCTOU / 幂等）

#### 结论
**未发现高危业务逻辑漏洞，存在一处低危竞态窗口**。

#### 证据
- **TOCTOU/竞态**：`backend/ddepdf` 在 `get_output_dir()` 中先 `realpath` 校验、再 `os.makedirs` 创建、再写入，目录创建与写入之间若被恶意替换为 symlink，仍可能逃逸。
  - 实际利用条件苛刻：需要攻击者能在 root 创建目录后、写入文件前的时间窗口内替换目录，本地普通用户难以稳定利用。
  - 严重度：**低**。
- **文件名唯一性**：文件名包含毫秒时间戳（`ts_ms = int((now - int(now)) * 1000)`），降低同秒覆盖概率。
- **打印机管理幂等**：postinst/prerm 对打印机存在性进行检测，重复安装/卸载不会失败。
- **autoOpen 逻辑**：基于文件列表 diff，避免刷新时重复打开旧文件。
- **索引边界检查**：`PdfPrinterModule::openPdfFile(int index)` 与 `deletePdfFile(int index)` 均校验 `index` 范围。

---

### D10 供应链

#### 结论
**未发现供应链漏洞**。

#### 证据
- **backend**：仅使用 Python 标准库（os/sys/time/pwd/shlex/configparser/subprocess），无第三方依赖。
- **C++ 插件/服务**：依赖系统提供的 Qt6、DTK6、DDE 控制中心开发库，均为 deepin 官方包。
- **构建系统**：CMake 配置中已加入 Stack Canary、FORTIFY、Full RELRO、路径脱敏等 hardening 标志。
- **打包脚本**：`ci/package-deb.sh` 正确设置 backend 权限 700，安装 control 脚本。
- **辅助脚本**：`scripts/collect_forum_posts.py` 依赖 `netfetch`（本地工具），仅用于调研，不影响运行安全。

---

## 四、发现汇总

### 4.1 未发现问题的维度

- D2 认证
- D4 反序列化
- D6 SSRF
- D7 加密 / 密钥

### 4.2 低风险发现

| 严重度 | 位置 | 问题 | 攻击场景 | 修复建议 |
|--------|------|------|----------|----------|
| 低 | `backend/ddepdf:108-109` | 非 root 兜底分支使用 `su -c` 创建目录 | 该分支仅在 backend 非 root 运行时触发，当前 deb 包安装为 700 root，实际不可达 | 保持现状；如未来需要非 root 运行，建议改用 `os.makedirs(..., exist_ok=True)` 并在创建前再次 realpath 校验 |
| 低 | `backend/ddepdf:82-96` | 目录创建与文件写入之间存在竞态窗口 | 攻击者需在 root 创建目录后、写入前替换为 symlink；窗口极短，难以稳定利用 | 使用 `os.open(outdir, O_NOFOLLOW|O_DIRECTORY)` 验证目录真实性，或在打开文件时使用 `O_NOFOLLOW`；作为纵深防御建议 |
| 低 | `scripts/collect_forum_posts.py` | 辅助脚本含网络请求 | 该脚本不参与运行时，仅用于开发调研；若误分发可能被误解 | 在打包时排除 scripts/ 目录，或在脚本顶部明确标注“开发调研工具，不随包分发” |

### 4.3 统计

- **高危**：0
- **中危**：0
- **低危**：3
- **安全/不适用维度**：7（D2、D4、D6、D7 及 D1/D3/D5/D8/D9/D10 中无问题的部分）

---

## 五、总结论

经过对 dde-pdf-printer 项目的独立白盒安全审计，**未发现高危或中危安全漏洞**。项目在以下方面表现良好：

1. **backend 权限隔离**：700 root 运行，输出目录严格限制在用户家目录内，并校验 realpath 防止符号链接逃逸。
2. **输入消毒**：CUPS title、文件名模板均经 `sanitize_filename` 处理，有效防止路径穿越和命令注入。
3. **命令执行安全**：QProcess 全部参数化调用，无 shell 拼接。
4. **无网络外连**：运行时代码无网络请求能力。
5. **无敏感信息泄露**：配置、日志、二进制均不含密钥、token 或构建环境敏感信息。
6. **二进制加固**：已启用 Stack Canary、FORTIFY、Full RELRO 和路径脱敏。

存在的 3 项低风险问题均属于纵深防御建议，在当前部署模型下难以被实际利用。建议在后续版本中：

- 对输出目录的创建与写入增加 `O_NOFOLLOW` / `O_DIRECTORY` 级别的原子校验，进一步缩窄竞态窗口。
- 在打包清单中明确排除 `scripts/` 等开发辅助文件，避免误进入分发包。

---

## 六、审计依据

- `backend/ddepdf`
- `src/service/configmanager.cpp/.h`
- `src/service/printermanager.cpp/.h`
- `src/service/outputdirwatcher.cpp/.h`
- `src/plugin/operation/pdfprintermodule.cpp/.h`
- `src/plugin/qml/*.qml`
- `src/plugin/CMakeLists.txt`
- `src/service/CMakeLists.txt`
- `ci/package-deb.sh`
- `debian/dde-pdf-printer.postinst/postrm/prerm`
- `docs/security-audit.md`