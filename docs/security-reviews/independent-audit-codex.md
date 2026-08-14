# 独立安全审计报告（codex 子 Agent）

> 审计项目：/home/master/Projects/deepin-pdf-printer
> 审计方法：dfyx_code_security_review「三层分析法 + 10 个安全维度」
> 审计时间：本次会话执行
> 审计范围：源码（src/、backend/、ci/、debian/、QML、脚本）及已构建的 deb 二进制，只读分析，未执行安装/打印。

---

## 1. 项目架构与输入面梳理

打印应用 -> CUPS -> /usr/lib/cups/backend/ddepdf (Python, root, 700)
                                |
                                v
                       写入 ~/.config/.../pdfprinter.conf 配置的输出目录
                                ^
                                |
DDE 控制中心插件 <- src/plugin/operation/pdfprintermodule.cpp + QML
                       |
                       v
                  src/service/ (ConfigManager / PrinterManager / OutputDirWatcher)

主要用户可控输入面：
- CUPS 打印参数：job、user、title、options、filename（backend/ddepdf 的 sys.argv）。
- 配置文件：~/.config/org.deepin.dde.pdfprinter/pdfprinter.conf（outputDir/filenameTemplate/keepTitleExtension）。
- 输出目录内的文件内容（PDF）。
- 控制中心插件中的目录选择结果、自动打开/删除列表索引。

---

## 2. 按 D1~D10 逐维度审计结论

### D1 注入（命令 / 模板 / SQL / 代码注入）

#### 发现 1：backend/ddepdf su 兜底分支存在命令注入（高）
- 严重度：高
- 位置：backend/ddepdf:107-110
- 证据：
  subprocess.run(['su', '-s', '/bin/sh', '-c',
                f'mkdir -p {shlex.quote(outdir)}', username],
               check=False)
- 攻击场景：username 来自 CUPS 传入的 sys.argv[2]，若 username 包含 shell 元字符（如 user$(id)），su 的 -c 参数整体使用 f-string 构造，但 username 未使用 shlex.quote，可能注入 shell。虽然该分支仅在 os.makedirs 因 PermissionError 失败时触发，但 root 后端调用 su 时以 root 权限执行命令，可导致任意命令执行。
- 修复建议：对 username 使用 shlex.quote(username)；或直接使用 Python 的 os.makedirs+os.chown 替代 su。

#### 发现 2：PrinterManager 命令参数硬编码，无命令注入（安全）
- 所有 lpadmin/lpstat 调用使用 QStringList 逐项传入，无 shell 解释。
- openPdfFile/openOutputDir 使用 QProcess::startDetached(program, args) 列表形式，不经过 shell。
- 结论：未发现命令注入漏洞。

#### 发现 3：filenameTemplate 模板注入无路径穿越风险（中）
- 严重度：中
- 位置：backend/ddepdf:178-191
- 证据：render_filename 对模板替换后的结果再次调用 sanitize_filename()，去掉路径分隔符；{jobid} 被强制替换为纯数字。
- 攻击场景：用户可配置模板为 {title}-{jobid}-{date}-{time}-../evil，经 sanitize 后变为 title-jobid-date-time-..-evil，无法穿越目录。
- 结论：不存在路径穿越。但建议增加模板白名单校验，避免生成过长或奇怪文件名。

### D2 认证

- 插件无独立认证机制，依赖 DDE 控制中心会话权限。
- backend/ddepdf 由 CUPS 以 root 调用，认证由 CUPS 调度器完成。
- 结论：未发现认证绕过漏洞。但 backend 默认信任 CUPS 传入的 user 参数，未校验用户是否真实存在即 chown/chdir。

### D3 授权 / 权限边界

#### 发现 4：backend 输出目录 realpath 校验存在 TOCTOU/绕过风险（高）
- 严重度：高
- 位置：backend/ddepdf:60-103
- 证据：
  - outputDir 配置经 os.path.expanduser/os.path.expandvars 后使用 realpath。
  - 校验条件：real_conf == real_home or real_conf.startswith(real_home + os.sep)。
- 攻击场景：
  1. 配置 /home/user/PDF/../../etc/cron.d 会被 realpath 解析为 /etc/cron.d，超出 real_home，会被拒绝。
  2. 但 realpath 校验与 os.makedirs/open 之间存在 TOCTOU：调用 realpath 后，目录组件可能被替换为符号链接，导致最终写入 /etc/cron.d 等系统目录。
- 修复建议：写入前使用 O_NOFOLLOW 打开目录文件描述符，并重新 realpath 校验；或采用 os.open(..., O_NOFOLLOW) 写入文件。

#### 发现 5：postrm 使用 /home/* 通配符清理配置（中）
- 严重度：中
- 位置：debian/dde-pdf-printer.postrm:9
- 证据：rm -rf /home/*/.config/org.deepin.dde.pdfprinter 2>/dev/null || true
- 攻击场景：purge 时遍历 /home/* 可能误删非预期用户目录下的配置；若系统用户家目录不在 /home 下则清理不到。虽不直接删除用户数据，但路径通配符方式不够安全。
- 修复建议：改为通过 getent passwd 遍历真实用户家目录后清理固定子目录。

### D4 反序列化

- 项目未使用 JSON/XML/Protobuf 反序列化不可信数据，无 pickle/jar 加载。
- backend/ddepdf 仅对 PDF 字节切片并写入文件，不做结构化解析。
- 结论：未发现反序列化相关漏洞。

### D5 文件操作（路径穿越 / 符号链接 / 临时文件）

#### 发现 6：backend 读取 CUPS 传入 filename 参数未限制路径（中）
- 严重度：中
- 位置：backend/ddepdf:215-218
- 证据：
  if filename and os.path.exists(filename):
      with open(filename, 'rb') as f:
          data = f.read()
- 攻击场景：filename 来自 CUPS sys.argv[6]。若 CUPS 或过滤器链传入任意路径，root 后端会读取该文件（如 /etc/shadow）。
- 修复建议：仅当 filename 位于 /var/spool/cups/tmp* 或 /tmp 下时才读取；否则忽略并只使用 stdin。

#### 发现 7：PrinterManager::openPdfFile / deletePdfFile 未限制文件名在输出目录内（低）
- 严重度：低
- 位置：src/service/printermanager.cpp:124-154
- 证据：openPdfFile(fileName) 使用 QDir(outputDir()).filePath(fileName)，未验证 fileName 是否位于输出目录内。
- 攻击场景：当前 C++ API 接收 int index，fileName 来自内部列表，正常路径安全；但 PrinterManager 接口本身接收字符串，若被其他调用者直接调用，可能路径穿越。
- 修复建议：确认 QFileInfo(path).canonicalPath() == QDir(outputDir()).canonicalPath()，否则拒绝。

#### 发现 8：postrm rmdir /home/*/PDF 仅删除空目录（安全）
- 仅删除空目录，不会递归删除用户 PDF 文件，符合保留用户数据的安全策略。
- 结论：无风险。

### D6 SSRF

- 项目无任何网络请求、URL 解析后发起 outbound HTTP/TCP/FTP 操作。
- dde-open/dde-file-manager 由用户本地触发。
- 结论：未发现 SSRF 漏洞。

### D7 加密 / 密钥

- 项目不涉及 TLS、加密、签名、密钥管理。
- 配置文件为普通 INI，无密码字段。
- 结论：未发现加密相关漏洞。

### D8 配置 / 敏感信息泄露

#### 发现 9：日志文件写入 ~/.cache/pdfprinter.log 未限制大小和权限（低）
- 严重度：低
- 位置：src/plugin/operation/pdfprintermodule.cpp:40-45、src/service/printermanager.cpp:17-22
- 证据：直接追加写入 QStandardPaths::CacheLocation + /pdfprinter.log，未设置文件权限，长期运行可能无限增长。
- 攻击场景：本地 DoS（磁盘耗尽）。
- 修复建议：设置文件权限为 600，并实现日志轮转或大小限制。

#### 发现 10：历史文件/目录名仍使用 deepin-pdf-printer/deepinpdf（低）
- 严重度：低
- 位置：debian/rules、debian/control、docs/ 等
- 证据：debian/rules 安装路径使用 deepin-pdf-printer；debian/control 描述含 Deepin。
- 结论：不构成安全漏洞，但属于命名审查风险，可能在打包/发布时造成混淆。

### D9 业务逻辑（竞态 / TOCTOU / 幂等）

#### 发现 11：backend 输出目录 TOCTOU 与 open 竞态（高，同 D3 发现 4）
- 严重度：高
- 位置：backend/ddepdf:90-103、233-243
- 证据：realpath 校验与 os.makedirs/open 之间存在时间窗口，目录组件可能被替换为符号链接。
- 修复建议：写入前重新 realpath 校验或使用 O_NOFOLLOW。

#### 发现 12：deletePdfFile 信号触发使用 const_cast（低）
- 严重度：低
- 位置：src/service/printermanager.cpp:163-169
- 证据：const_cast<PrinterManager *>(this)->pdfFilesChanged();
- 结论：代码风格问题，不导致安全漏洞。

### D10 供应链 / 依赖版本

- 依赖 Qt6、DTK6、deepin 控制中心、cups-filters、ghostscript，均来自 deepin 25 系统包。
- CI 使用 debootstrap + qemu-user-static，打包使用 dpkg-deb --root-owner-group。
- 结论：未发现依赖版本锁定缺失或已知 CVE 引入。建议 CI 中增加依赖版本/SBOM 校验。

---

## 3. 发现汇总

| 严重度 | 数量 | 编号 |
|--------|------|------|
| 高 | 3 | D1-发现1、D3-发现4、D9-发现11 |
| 中 | 3 | D1-发现3、D3-发现5、D5-发现6 |
| 低 | 5 | D5-发现7、D8-发现9、D8-发现10、D9-发现12、D5-发现8 |
| 安全 | 5 | D2、D4、D6、D7、D10 无发现 |

总计：8 个发现（高 3 / 中 3 / 低 2）+ 3 个低风险信息项。

---

## 4. 总体结论

本项目整体安全设计思路正确：
- backend 以 700 root 运行，对输出目录做了 realpath 边界校验，防止用户配置将 PDF 写到系统目录。
- 命令执行范围受限为 lpadmin、lpstat、su(mkdir)、dde-open、dde-file-manager。
- 模板渲染后再次 sanitize_filename，避免文件名注入。
- 插件侧无 QFileDialog 等禁止操作，目录选择通过 D-Bus 调用系统文件对话框。

但存在以下需要修复的关键问题：
1. backend/ddepdf 中 su 兜底分支对 username 参数缺乏 shlex.quote，存在命令注入风险。
2. backend/ddepdf 中 realpath 校验与文件写入之间存在 TOCTOU 窗口。
3. backend/ddepdf 读取 CUPS 传入的 filename 参数时未限制路径，root 后端可能读取任意文件。
4. postrm 使用 /home/* 通配符清理配置，存在误删风险。

修复上述问题后，项目可认为满足 deepin 插件大赛及日常使用的安全要求。

---

报告生成方式：白盒静态审计，未修改源码，未执行任何打印/安装操作。
