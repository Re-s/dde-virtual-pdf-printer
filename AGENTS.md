# AGENTS.md — AI 二次开发引导

> 本文件供 AI 编程助手（Cursor / Claude Code / Codex / Hermes 等）在接手本项目时自动加载。
> 目标是让 AI 在**不重踩前人坑**的前提下快速理解架构、正确构建、安全扩展。

---

## 1. 项目是什么

DDE（deepin 桌面环境）的**虚拟 PDF 打印机**，类似 Windows「Microsoft Print to PDF」：

- 任意应用的打印对话框 → 选择 **Deepin-PDF** 打印机 → 输出 PDF 到用户目录
- 通过 **DDE 控制中心插件** 提供图形化管理（4 个页面）
- 参赛作品（deepin 插件大赛），基于 deepin Skills 开发

## 2. 架构速览

```
打印对话框 → CUPS → backend/deepinpdf (Python, root) → <输出目录>/*.pdf
                                                        ↑ 读同一配置
控制中心 → src/plugin/ (C++ PdfPrinterModule + QML 4页) → ConfigManager (QSettings)
              ↓
          src/service/ (PrinterManager / ConfigManager / OutputDirWatcher)
```

三个层次，各自独立：

| 层 | 位置 | 职责 | 运行身份 |
| --- | --- | --- | --- |
| CUPS backend | `backend/deepinpdf` | 收打印数据 → 写 PDF 文件 | **root**（权限 700） |
| 服务层 | `src/service/` | 打印机管理、配置、目录监听（纯 Qt 静态库） | 用户会话 |
| 控制中心插件 | `src/plugin/` | DccObject + QML 页面，调用服务层 | 用户会话 |

**配置流**：控制中心设置 → QSettings 写入 `~/.config/org.deepin.dde.pdfprinter/pdfprinter.conf` → backend 与插件**各自读取同一文件**（backend 用 Python configparser，插件用 QSettings）。改目录后两边自动同步。

## 3. 环境与构建（deepin 25）

```bash
# 编译环境（一次性）
sudo apt install -y build-essential cmake git \
  qt6-base-dev qt6-declarative-dev qt6-tools-dev linguist-qt6 libxkbcommon-dev \
  libdtk6core-dev libdtk6gui-dev libdtk6widget-dev \
  dde-control-center-dev cups cups-filters ghostscript

# 构建插件
cmake -S src/plugin -B build/integration -DCMAKE_INSTALL_PREFIX=/usr
cmake --build build/integration -j$(nproc)
```

**⚠️ 构建三铁律（项目特有）**：

1. **cmake 用 `/usr/local/bin/cmake`**（wrapper，自动 `env -u LD_LIBRARY_PATH`；裸 `cmake` 在 Hermes/沙箱环境会被污染的 libssl 炸掉）
2. **改 QML 必须 `rm -rf build/integration` 完整重建**：QML 会被编译进 `libpdfprinter_qml.so`（qmlcache），增量 make 不感知 QML 源码变化 → 旧逻辑仍在
3. **打包漏 qmldir/qmltypes 会导致插件静默不加载**：`cp` 时 `*.qml` 之外还要带 `qmldir` 和 `*_qml.qmltypes`

## 4. 打包与安装

```bash
# 完整打包流程见 README「构建」章节
# 关键点：
# - backend 必须 install -m 700（755 → CUPS 以 lp 用户运行 → 读不了用户 700 的 ~/.config）
# - dpkg-deb --root-owner-group（ostree 只读系统只认 dpkg 写路径）
# - postinst 自动创建打印机（lpadmin），prerm 删除
```

**安装后验证**：
```bash
dpkg -i deepin-pdf-printer_*.deb     # 应输出「已创建打印机 Deepin-PDF」
lpstat -p Deepin-PDF                 # 打印机存在
lp -d Deepin-PDF somefile.txt        # 打印 → ~/PDF/ 或配置目录出现 .pdf
```

## 5. 关键技术约束（踩坑总结，务必遵守）

### 5.1 控制中心是纯 QML 应用（QGuiApplication）
- **禁止 `QFileDialog`**（QtWidgets 模块）→ 无 QApplication 直接崩溃
- **禁止 QML `FolderDialog`**（QtQuick.Dialogs）→ 被 dde-file-dialog 接管，行为不可控
- **目录选择正确姿势**：D-Bus 调 `com.deepin.filemanager.filedialog` 服务：
  `createDialog(key)` → `setFileMode(2)` (Directory) → `show()` → 监听 `accepted` 信号 → 调 `selectedUrls` 方法取结果
  - ⚠️ `selectedUrls` 是**方法**不是信号！真正的信号是 `accepted`/`rejected`
  - ⚠️ createDialog 的 **key 必须纯字母数字**（连字符/下划线 → NoReply 超时）
  - ⚠️ **必须异步**（QML 调 C++ 时用信号回传，`QEventLoop` 阻塞主线程 → 界面卡死）

### 5.2 Q_PROPERTY 的 WRITE 方法不能当 QML 函数调
```qml
// ❌ dccData.setOutputDir(dir)     → TypeError: not a function
// ✅ dccData.outputDir = dir       （属性赋值）
```
同理 `setAutoOpen` → `dccData.autoOpen = checked`。`Q_INVOKABLE` 方法（createPrinter 等）才能当函数调。

### 5.3 DccObject 页面结构
- `pageType: DccObject.Item` 的 DccObject **必须有 `page:` 组件**，否则页面空白
- 展示文本行：`pageType: DccObject.Editor` + `page: Text {...}`（与状态页同构）
- Editor 行的 Text 要**包一层 Item + anchors.fill + wrapMode**，否则窄窗口文字重叠
- 删除确认用 `Popup`（Controls 2.0 有）；**`Dialog` 在 Controls 2.0 不存在**（"Dialog is not a type"）

### 5.4 CUPS backend（root 运行）
- 调用约定：`backend job user title num-copies options [filename]`
- 权限 **700 root:root**；`pwd.getpwnam(username)` 拿用户 home/uid/gid，`chown` 输出文件
- 输出目录从 `~/.config/org.deepin.dde.pdfprinter/pdfprinter.conf` 读 `[General] outputDir`，未配置回退 `~/PDF`
- 文件名 `sanitize_filename(title)` 防穿越 + 毫秒时间戳防同秒覆盖 + 写失败 try/except 返回 1
- **禁止网络、禁止读用户敏感文件**（安全审查红线）

### 5.5 其他
- deepin 25 是 ostree：`/usr` 只读，系统文件只能通过 deb 包安装
- 控制中心插件路径：`/usr/lib/x86_64-linux-gnu/dde-control-center/plugins_v1.1/<name>/`
- 插件懒加载：控制中心导航到模块才 dlopen（`/proc/<pid>/maps` 可查加载）
- QML 磁盘缓存：`~/.cache/deepin/dde-control-center/qmlcache/`，改 QML 后建议清掉

## 6. 二次开发常见任务指引

| 想做什么 | 改哪里 |
| --- | --- |
| 加一个设置项 | `src/service/configmanager.{h,cpp}`（键/读写）+ `PdfprinterSettingsPage.qml` + backend 读取 |
| 改文件列表展示 | `PdfprinterFilesPage.qml` + `pdfFileDetails`（`pdfprintermodule.cpp`） |
| 新增页面 | 新建 `PdfprinterXxxPage.qml` → `PdfprinterMain.qml` 注册（weight 递增） |
| 改打印机行为 | `src/service/printermanager.cpp`（QProcess 调 lpadmin/lpstat） |
| 改输出文件名规则 | `backend/deepinpdf`（sanitize_filename + 命名模板） |

**修改后必做**：
1. `rm -rf build/integration && cmake ... && make`（QML 变更时必须）
2. `lupdate qml/ -ts translations/pdfprinter_zh_CN.ts` + 去掉 unfinished 标记 + `lrelease`（翻译变更时）
3. 重新打包 deb → dpkg -i → 控制中心实测

## 7. 验证清单（改动自测）

```bash
# 静态
/usr/lib/qt6/bin/qmllint src/plugin/qml/*.qml     # QML 语法
python3 -m py_compile backend/deepinpdf            # backend 语法

# 运行时（控制中心 D-Bus）
dbus-send --session --dest=org.deepin.dde.ControlCenter1 --type=method_call --print-reply \
  /org/deepin/dde/ControlCenter1 org.deepin.dde.ControlCenter1.GetAllModule   # 模块树
dbus-send --session --dest=org.deepin.dde.ControlCenter1 --type=method_call --print-reply \
  /org/deepin/dde/ControlCenter1 org.deepin.dde.ControlCenter1.ShowPage string:pdfprinter/<page>

# 端到端
lp -d Deepin-PDF /path/to/file && ls -t ~/PDF/ | head -1   # 打印 → PDF
```

## 8. 安全红线（评审/参赛要求）

- ❌ 无任何网络请求（socket/curl/http）——功能纯本地
- ❌ 不读 /etc/passwd、/proc、用户文档（除输出目录内 *.pdf）
- ✅ 命令执行白名单：仅 lpadmin / lpstat / su(mkdir)
- ✅ 文件写入仅限用户配置的输出目录
- 新增代码保持同等约束，否则评审不过

## 9. 许可证与参赛

- GPL-3.0-or-later；基于 deepin Skills 开发，参赛作品
- 文档：`docs/design.md`（架构）、`docs/security-audit.md`（安全审查）、`docs/forum-print-survey.md`（需求调研）
