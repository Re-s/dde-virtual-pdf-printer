# 安全审查报告 — deepin-pdf-printer

> 审查日期：2026-08-12 | 版本：v0.4.9 基线（后续版本持续适用）
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
| backend 写 outpath | `<输出目录>/<清洗文件名>-<jobid>-<毫秒时间戳>.pdf` | 否，仅用户配置目录 |
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
