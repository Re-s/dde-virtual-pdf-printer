# DDE 虚拟 PDF 打印机 — 项目框架设计

> 参赛方向：① dde-control-center-development（控制中心插件）
> 核心创意：为 deepin 提供 Windows「Microsoft Print to PDF」等价能力
> 状态：POC 已验证通过（2026-08-12），进入正式开发

## 1. 项目目标

在 deepin/UOS v25 上实现「任意应用 → 打印对话框 → 选 DDE-PDF → 输出 PDF 到用户目录」的完整闭环，并通过 **DDE 控制中心插件** 提供图形化管理界面。

## 1.5 需求调研（2026-08-12，论坛数据支撑）

> 方法：扫描 bbs.deepin.org 公开时间线 API 共 2000 帖，本地关键词过滤「打印/PDF/打印机/CUPS/扫描」等，命中 27 个相关主题。
> 完整数据：`docs/forum-print-survey.md`、`docs/forum-print-posts.json`

### 需求验证结论

| 维度 | 结论 | 证据 |
|------|------|------|
| 需求真实性 | ✅ 用户主动提出「装个 PDF 虚拟打印机」 | 帖 300247 |
| 痛点强度 | ✅ 打印列为新用户迁移三大障碍之首 | 帖 300634/300620 |
| 差异化 | ✅ 论坛无同类作品/教程 | 全样本扫描 |
| 目标用户 | ✅ Windows 迁移者、办公用户 | 帖 300634 原话 |

### 关键用户原话

- 帖 [300247](https://bbs.deepin.org/post/300247)：**「先装个 PDF 虚拟打印机，打成 PDF 文件，然后就可以随便打印了」** —— 用户需要该能力但 deepin 未内置
- 帖 [300634](https://bbs.deepin.org/post/300634)：**「如果能解决几个问题，很多新人会拿来办公的。驱动：打印机、显卡、无线网卡」** —— 打印体验是新用户办公门槛之首

### 相关帖子 URL 列表（27 个命中主题）

| # | 帖子链接 | 命中词 |
|---|---------|--------|
| 1 | https://bbs.deepin.org/post/300713 | pdf, PDF |
| 2 | https://bbs.deepin.org/post/300247 | 打印, 打印机, pdf, PDF, **虚拟打印** |
| 3 | https://bbs.deepin.org/post/300695 | 打印, 打印机 |
| 4 | https://bbs.deepin.org/post/300686 | pdf, PDF |
| 5 | https://bbs.deepin.org/post/300676 | print, Print, cups, CUPS |
| 6 | https://bbs.deepin.org/post/300675 | 打印, cups, CUPS |
| 7 | https://bbs.deepin.org/post/300665 | 扫描 |
| 8 | https://bbs.deepin.org/post/300634 | 打印, 打印机 |
| 9 | https://bbs.deepin.org/post/300632 | print, Print |
| 10 | https://bbs.deepin.org/post/300623 | 扫描 |
| 11 | https://bbs.deepin.org/post/300620 | 打印, 打印机 |
| 12 | https://bbs.deepin.org/post/300553 | 扫描 |
| 13 | https://bbs.deepin.org/post/300580 | print, Print |
| 14 | https://bbs.deepin.org/post/300555 | 打印, cups, CUPS |
| 15 | https://bbs.deepin.org/post/300507 | 打印, cups, CUPS |
| 16 | https://bbs.deepin.org/post/286282 | 打印, 扫描 |
| 17 | https://bbs.deepin.org/post/298294 | 打印 |
| 18 | https://bbs.deepin.org/post/300508 | pdf, PDF |
| 19 | https://bbs.deepin.org/post/300499 | cups, CUPS |
| 20 | https://bbs.deepin.org/post/300473 | pdf, PDF |
| 21 | https://bbs.deepin.org/post/300482 | 扫描 |
| 22 | https://bbs.deepin.org/post/300082 | print, Print |
| 23 | https://bbs.deepin.org/post/300460 | pdf, PDF, 扫描 |
| 24 | https://bbs.deepin.org/post/300456 | 扫描 |
| 25 | https://bbs.deepin.org/post/300438 | 扫描 |
| 26 | https://bbs.deepin.org/post/300432 | pdf, PDF, print, Print, cups, CUPS |
| 27 | https://bbs.deepin.org/post/300431 | 打印, pdf, PDF, cups, CUPS, 扫描 |

> ⚠️ 部分命中为误报（如 300632 是声卡问题、300623 是 DLNA 投屏），已剔除的误报不计入结论；高价值证据为 #2（虚拟打印）、#8/#11（新用户办公障碍）、#26/#27（deepin 25.2.1 打印修复公告）。

## 2. 总体架构

```
┌──────────────────────────────────────────────────────────┐
│                  DDE 控制中心 (dde-control-center)       │
│  ┌────────────────────────────────────────────────────┐  │
│  │  插件: dde-pdf-printer (QML 页面 + C++ 逻辑)     │  │
│  │  ├─ 状态页: 打印机检测/创建/删除                     │  │
│  │  ├─ 配置页: 输出目录、默认文件名、自动打开           │  │
│  │  ├─ 列表页: 已生成 PDF 浏览/打开/删除               │  │
│  │  └─ 帮助页: 使用引导（如何打印到 PDF）              │  │
│  └───────────────┬────────────────────────────────────┘  │
│                  │ D-Bus / lpadmin / 文件系统             │
└──────────────────┼───────────────────────────────────────┘
                   │
┌──────────────────▼───────────────────────────────────────┐
│  CUPS 子系统                                            │
│  ┌──────────────────────────────┐  ┌──────────────────┐  │
│  │ 打印机 "DDE-PDF"          │  │ 自定义 backend   │  │
│  │ (Generic-PDF_Printer PPD)    │──│ ddepdf        │  │
│  │                              │  │ (Python, root)   │  │
│  └──────────────────────────────┘  └────────┬─────────┘  │
│                                             │ 写入        │
└─────────────────────────────────────────────┼────────────┘
                                              ▼
                                    ~/PDF/<title>-<jobid>-<ts>.pdf
```

## 3. 模块划分

### 3.1 backend（已 POC 验证）— `backend/ddepdf`

| 职责 | 实现 | 状态 |
| --- | --- | --- |
| 接收 CUPS 打印数据 | Python, 参数 job/user/title/copies/options | ✅ |
| 剥离 PJL 包装头 | `data.find(b'%PDF')` | ✅ |
| 写入用户目录 | root 创建 + chown 给用户 | ✅ |
| 设备发现 | discover() 输出 | ✅ |

**已实现的增强（2026-08）**：
- ✅ 输出目录可配置（控制中心设置页 → 原生目录选择器，QSettings 存储）
- ✅ 文件名模板可配置（`{title}`/`{jobid}`/`{date}`/`{time}` 占位符 + 保留原后缀开关）
- 多用户并发安全（已有 jobid 区分）

### 3.2 打印机管理服务 — `src/service/`（C++ / QProcess）

| 组件 | 职责 |
| --- | --- |
| `PrinterManager` | 检测打印机存在（lpstat）、创建（lpadmin）、删除 |
| `OutputDirWatcher` | 监听输出目录文件变化（QFileSystemWatcher） |
| `ConfigManager` | DConfig 读写（输出目录、文件名规则、自动打开） |
| `PolkitHelper` | 需要 root 的操作（lpadmin 创建时 polkit 授权） |

### 3.3 控制中心插件 — `src/plugin/`（QML + C++）

遵循 `dde-control-center-development` 技能规范：

```
src/plugin/
├── dcc-pdf-printer-plugin.cpp   # DCC_FACTORY_CLASS 注册
├── dcc-pdf-printer-plugin.h
├── model/
│   ├── PdfFileModel.h/.cpp      # QAbstractListModel: PDF 文件列表
│   └── PrinterModel.h/.cpp      # 打印机状态模型
├── qml/
│   ├── Main.qml                 # 插件根页面（模块元数据）
│   ├── PrinterPage.qml          # 状态/创建/删除
│   ├── SettingsPage.qml         # 配置
│   ├── FileListPage.qml         # PDF 列表
│   └── HelpPage.qml             # 使用引导
└── resources/
    ├── icons/                   # DCI 图标
    └── translations/            # 中英翻译
```

### 3.4 打包与安装 — `debian/`

| 文件 | 职责 |
| --- | --- |
| control | 包元数据、依赖（cups, cups-filters, ghostscript, dde-control-center） |
| rules | dh 构建规则，backend 700 权限 + root 属主 |
| postinst | 自动创建打印机（首次安装）、polkit 规则 |
| prerm | 删除打印机、清理 |

## 4. 关键技术决策

| 决策点 | 方案 | 理由 |
| --- | --- | --- |
| backend 权限 | **700 root:root** | CUPS 以 root 运行，可写任意用户目录 + chown |
| 输出目录 | `~/PDF`（默认）+ DConfig 可改 | 与 cups-pdf 习惯一致 |
| 打印机创建时机 | 安装时 postinst + 插件内按钮 | 双保险 |
| PPD | 系统自带 Generic-PDF_Printer-PDF.ppd | 无需自带 PPD，减少维护 |
| 文件命名 | 模板 `{title}-{jobid}-{date}-{time}.pdf`（可配置，支持保留原后缀） | 避免重名覆盖 + 用户可定制 |
| 权限模型 | lpadmin 用户级（lp 组）+ polkit 兜底 | 普通用户加入 lp 组后即可，无需 root |
| 插件语言 | C++ (DccObject) + QML | 控制中心 v25 标准架构 |

## 5. 目录结构（最终）

```
dde-pdf-printer/
├── backend/
│   └── ddepdf              # CUPS backend 脚本（Python）
├── src/
│   ├── service/               # 打印机管理服务（C++）
│   └── plugin/                # 控制中心插件（C++ + QML）
├── data/
│   ├── org.deepin.dde.pdffile-printer.json   # DConfig meta
│   └── *.desktop
├── docs/
│   └── design.md              # 设计文档
├── debian/                    # 打包配置
├── CMakeLists.txt             # 顶层构建
├── README.md                  # 项目说明（参赛提交）
└── LICENSE                    # GPL-3.0-or-later
```

## 6. 里程碑

| 阶段 | 内容 | 预计 |
| --- | --- | --- |
| ✅ POC | backend + 打印机 + 打印出 PDF | 已完成 |
| M1 | 打印机管理服务（C++）：检测/创建/删除/配置 | 1 天 |
| M2 | 控制中心插件骨架（DCC_FACTORY_CLASS + 页面导航） | 1 天 |
| M3 | 插件核心功能：状态页 + 创建/删除 + 配置页 | 1.5 天 |
| M4 | PDF 列表页 + 文件操作（打开/删除/目录） | 1 天 |
| M5 | 打磨：翻译、DCI 图标、搜索索引、主题适配 | 1 天 |
| M6 | 打包（deb 安装完整测试）+ 文档 + 演示材料 | 1 天 |
| M7 | 参赛提交（GitHub 开源 + 论坛发帖） | 0.5 天 |

**总计约 7 天**（9/9 截止前充裕）

## 7. 风险与对策

| 风险 | 等级 | 对策 |
| --- | --- | --- |
| 控制中心插件 API 与示例差异 | 中 | 参考官方 plugin-example + evals 测试用例 |
| polkit 授权流程复杂 | 低 | lp 组用户无需授权，仅兜底 |
| DConfig 集成 | 低 | 技能有完整参考资料 |
| 多用户场景 | 低 | backend 按 user 参数定位 home |

## 8. 已验证的 POC 结果

```
$ lp -d DDE-PDF /tmp/test.txt
请求 ID 为 DDE-PDF-5
$ ls ~/PDF/
test-print.txt-5-20260812-133027.pdf   ← PDF document, 1 page ✅
```

- backend 以 700 root 权限安装于 `/usr/lib/cups/backend/ddepdf`
- 打印机 `DDE-PDF` 使用系统 Generic-PDF_Printer PPD
- 中文内容、gs 渲染、文件属主均验证通过
