# 【deepin插件开发活动】DDE 虚拟 PDF 打印机 —— 让 deepin 拥有「Microsoft Print to PDF」

> **作品名称**：DDE 虚拟 PDF 打印机（dde-virtual-pdf-printer）
> **参赛方向**：方向一 · 控制中心插件
> **源码仓库**：https://github.com/Re-s/dde-virtual-pdf-printer
> **作品类型**：CUPS 虚拟打印机（backend）+ DDE 控制中心插件

---

## 一、作品简介

在 deepin/UOS v25 上实现类似 Windows「Microsoft Print to PDF」的完整闭环：

**任意应用 → 打印对话框 → 选择「Deepin-PDF」→ 输出 PDF 到指定目录**

并通过 DDE 控制中心插件提供图形化管理界面（打印机状态、PDF 文件管理、输出目录配置、使用帮助），安装即用。

> 💡 **需求来源**：论坛用户真实痛点。调研 2000 帖发现，用户迁移到 deepin 后**打印机支持是办公障碍第一名**（帖子 300634 / 300620），并有用户直接建议「装个 PDF 虚拟打印机」（帖子 300247）。详见 [需求调研文档](https://github.com/Re-s/dde-virtual-pdf-printer/blob/master/docs/forum-print-survey.md)。

## 二、功能特性

- ✅ **任意应用打印为 PDF**：WPS / 浏览器 / LibreOffice 打印对话框直接可选
- ✅ **控制中心管理模块**「PDF 打印机」4 个页面：
  - 📊 打印机状态：一键安装 / 移除打印机
  - 📄 PDF 文件列表：浏览（名称/大小/时间）、打开、删除（带确认）、打开目录
  - ⚙️ 设置：自定义输出目录（deepin 原生目录选择器）、打印后自动打开 PDF
  - ❓ 帮助：三步使用引导 + 常见问题
- ✅ **输出目录可自定义**：插件与 backend 统一读配置，修改后全局生效
- ✅ **安装即用**：deb 包 postinst 自动创建打印机
- ✅ **中文界面** + DCI 图标，深度融入 DDE 设计语言

## 三、截图

| 打印机状态 | PDF 文件列表 |
| --- | --- |
| ![状态](https://github.com/Re-s/dde-virtual-pdf-printer/releases/download/v0.5.4/01-status.png) | ![文件列表](https://github.com/Re-s/dde-virtual-pdf-printer/releases/download/v0.5.4/02-files.png) |

| 设置 | 帮助 |
| --- | --- |
| ![设置](https://github.com/Re-s/dde-virtual-pdf-printer/releases/download/v0.5.4/03-settings.png) | ![帮助](https://github.com/Re-s/dde-virtual-pdf-printer/releases/download/v0.5.4/04-help.png) |

## 四、安装方式

### 方式一：安装 Release deb 包（推荐）

```bash
wget https://github.com/Re-s/dde-virtual-pdf-printer/releases/download/v0.5.4/deepin-pdf-printer_0.5.4_amd64.deb
sudo dpkg -i deepin-pdf-printer_0.5.4_amd64.deb
```

安装完成即自动创建「Deepin-PDF」打印机，打开任意应用打印对话框即可使用。

### 方式二：从源码构建

```bash
# 编译环境
sudo apt install -y build-essential cmake git \
  qt6-base-dev qt6-declarative-dev qt6-tools-dev linguist-qt6 libxkbcommon-dev \
  libdtk6core-dev libdtk6gui-dev libdtk6widget-dev dde-control-center-dev \
  cups cups-filters ghostscript

# 构建 + 打包（完整步骤见仓库 README）
git clone https://github.com/Re-s/dde-virtual-pdf-printer.git
cd dde-virtual-pdf-printer
cmake -S src/plugin -B build/integration -DCMAKE_INSTALL_PREFIX=/usr
cmake --build build/integration -j$(nproc)
# ... 按 README「构建」章节打包 deb
```

## 五、使用说明

1. 打开任意文档（WPS、浏览器、LibreOffice 等）→ 打印（Ctrl+P）
2. 打印机选择 **Deepin-PDF** → 打印
3. PDF 保存到默认 `~/PDF/`（可在控制中心设置页修改输出目录）
4. 控制中心「PDF 打印机」→ 管理生成的 PDF 文件

## 六、开发过程说明（deepin Skills 使用）

### 使用的 Skill 模块

| Skill | 用途 |
| --- | --- |
| `dde-control-center-development` | 控制中心插件开发规范：DccObject 页面结构、DCC_FACTORY_CLASS 注册、插件打包（dcc_install_plugin）、翻译处理 |
| `dtk-development` | DTK6 应用框架：DCI 图标规范、QSettings 配置、系统集成 |
| `dde-shell-development` | DDE 生态集成认知（插件加载机制、懒加载行为） |
| `deepin-25-platform` | deepin 25 平台特性：ostree 只读系统、dpkg 打包写路径、CUPS 集成 |

### 关键技术点（开发过程中解决的难题）

1. **ostree 不可变系统适配**：deepin 25 的 `/usr` 只读，CUPS backend 无法手动拷贝 → 用 **deb 包 + `--root-owner-group`** 走 dpkg 专用写路径
2. **CUPS backend 权限模型**：backend 带 world 执行位会以 `lp` 用户运行（读不了用户 700 的配置）→ **权限 700 强制 root 运行**，脚本内用 `pwd.getpwnam` 定位用户并 chown 输出文件
3. **纯 QML 应用的目录选择**：控制中心是 QGuiApplication，`QFileDialog`（Widgets）直接崩溃、QML `FolderDialog` 被 dde-file-dialog 接管行为不可控 → **D-Bus 异步调用 `com.deepin.filemanager.filedialog`**（createDialog → setFileMode(Directory) → accepted 信号 → selectedUrls）
4. **QML 编译缓存陷阱**：QML 被编译进 `lib<name>_qml.so`，改 QML 必须完整重建，否则旧代码生效（增量 make 不感知）
5. **Q_PROPERTY WRITE 陷阱**：`setOutputDir` 是属性 WRITE 方法，QML 中不能当函数调（TypeError）→ 用属性赋值 `dccData.outputDir = dir`

### 开发模式

采用 **AI 辅助模块化开发**：先 POC 验证 CUPS backend 可行性 → 定义接口契约（`docs/contract.md`）→ 服务层 / 插件 C++ / 插件 QML 三模块并行开发 → 集成验证 → deb 打包全链路测试。整个开发过程由 AI 编程工具 + deepin Skills 协作完成，对话记录截图见下。

### AI 工具调用 deepin Skills 对话记录截图

![AI 对话记录](https://github.com/Re-s/dde-virtual-pdf-printer/releases/download/v0.5.4/05-ai-dialogue.png)

> 上图展示 AI 编程工具调用 deepin Skills（dde-control-center-development / dtk-development / deepin-25-platform）辅助开发的关键过程：POC 验证 → 契约化并行开发 → 目录选择闪退修复（D-Bus 方案）→ 回写修复（WRITE 属性/QML 缓存）→ P0 增强与发布。

## 七、安全说明

- ✅ **无任何网络请求**（纯本地功能）
- ✅ **无敏感数据采集**（不读系统文件，仅操作输出目录）
- ✅ 路径穿越 / 命令注入防护已实测（见 `docs/security-audit.md`）
- ✅ 开源协议：**GPL-3.0-or-later**（OSI 批准）

## 八、项目文档

| 文档 | 链接 |
| --- | --- |
| README（功能/安装/构建） | https://github.com/Re-s/dde-virtual-pdf-printer |
| AI 二次开发引导（AGENTS.md） | https://github.com/Re-s/dde-virtual-pdf-printer/blob/master/AGENTS.md |
| 架构设计 | https://github.com/Re-s/dde-virtual-pdf-printer/blob/master/docs/design.md |
| 需求调研（2000 帖分析） | https://github.com/Re-s/dde-virtual-pdf-printer/blob/master/docs/forum-print-survey.md |
| 安全审查报告 | https://github.com/Re-s/dde-virtual-pdf-printer/blob/master/docs/security-audit.md |

**Release**：https://github.com/Re-s/dde-virtual-pdf-printer/releases/tag/v0.5.4 （deb 包 + 截图）
