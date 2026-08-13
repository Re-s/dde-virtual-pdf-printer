# AGENTS.md — AI 二次开发引导

> 本文件供 AI 编程助手（Cursor / Claude Code / Codex / Hermes 等）在接手本项目时自动加载。
> 目标是让 AI 在**不重踩前人坑**的前提下快速理解架构、正确构建、安全扩展。

---

## 1. 项目是什么

DDE（deepin 桌面环境）的**虚拟 PDF 打印机**，类似 Windows「Microsoft Print to PDF」：

- 任意应用的打印对话框 → 选择 **DDE-PDF** 打印机 → 输出 PDF 到用户目录
- 通过 **DDE 控制中心插件** 提供图形化管理（4 个页面）
- 参赛作品（deepin 插件大赛），基于 deepin Skills 开发

## 2. 架构速览

```
打印对话框 → CUPS → backend/ddepdf (Python, root) → <输出目录>/*.pdf
                                                        ↑ 读同一配置
控制中心 → src/plugin/ (C++ PdfPrinterModule + QML 4页) → ConfigManager (QSettings)
              ↓
          src/service/ (PrinterManager / ConfigManager / OutputDirWatcher)
```

三个层次，各自独立：

| 层 | 位置 | 职责 | 运行身份 |
| --- | --- | --- | --- |
| CUPS backend | `backend/ddepdf` | 收打印数据 → 写 PDF 文件 | **root**（权限 700） |
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
dpkg -i dde-pdf-printer_*.deb     # 应输出「已创建打印机 DDE-PDF」
lpstat -p DDE-PDF                 # 打印机存在
lp -d DDE-PDF somefile.txt        # 打印 → ~/PDF/ 或配置目录出现 .pdf
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
- **删除按钮不要用 Popup 确认**：Popup 声明在 `DccObject` 根（逻辑对象，非可视 Item）上时
  `anchors.centerIn: parent` 的 parent 不可见 → 弹窗不显示 → 用户以为按钮失效（实测踩坑）。
  恢复为直接调用 `dccData.deletePdfFile(index)`（列表自动刷新）。
  `Dialog` 在 Controls 2.0 不存在（"Dialog is not a type"）

### 5.4 CUPS backend（root 运行）
- 调用约定：`backend job user title num-copies options [filename]`
- 权限 **700 root:root**；`pwd.getpwnam(username)` 拿用户 home/uid/gid，`chown` 输出文件
- 输出目录从 `~/.config/org.deepin.dde.pdfprinter/pdfprinter.conf` 读 `[General] outputDir`，未配置回退 `~/PDF`
- 文件名 `sanitize_filename(title)` 防穿越 + 毫秒时间戳防同秒覆盖 + 写失败 try/except 返回 1
- **禁止网络、禁止读用户敏感文件**（安全审查红线）

### 5.5 其他
- deepin 25 是 ostree：`/usr` 只读，系统文件只能通过 deb 包安装
- 控制中心插件路径：`/usr/lib/x86_64-linux-gnu/dde-control-center/plugins_v1.1/<name>/`
  （**deepin 25 用 v1.1；deepin 23/beige 源的 dcc 宏输出 v1.0**——安装位置随构建环境的
  `dcc_build_plugin` 宏版本，装错目录插件不加载，用 `GetAllModule` 或 `/proc/<pid>/maps` 验证）
- 配置键：`outputDir` / `filenameTemplate`（默认 `{title}-{jobid}-{date}-{time}`）/
  `keepTitleExtension`（默认 false）/ `autoOpen`
- 插件懒加载：控制中心导航到模块才 dlopen（`/proc/<pid>/maps` 可查加载）
- QML 磁盘缓存：`~/.cache/deepin/dde-control-center/qmlcache/`，改 QML 后建议清掉

### 5.6 系统级事故教训（2026-08 实测）
- **多架构（arm64）撤销会连带卸载系统组件**：`apt-get install -y -f` 修复 broken 依赖时，
  可能把 `dde-daemon`/`dde-control-center` 一起卸掉（依赖链断裂）。撤销后必须验证
  `dpkg -l dde-daemon dde-control-center` 仍是 `ii`，否则控制中心直接消失。
- **bind mount 残留卡 dpkg**：若 `/usr/libexec/dde-daemon/keybinding/*` 下有「设备或资源忙」
  无法删除的文件，先 `mount | grep <路径>` 检查是否是独立挂载点 → `sudo umount` 后再删。
  这类残留来自调试时用 `mount --bind` 覆盖系统脚本（如 GrandSearch noop 测试）。
- **卸载插件后控制中心残留进程仍在内存**：插件文件删除后，运行中的控制中心仍显示模块
  （.so 在内存），但图标/悬停态因文件缺失显示空白——重装插件 + 重启控制中心即可恢复。
- **deepin-immutable-ctl 与 dpkg 冲突**：`/usr` 是 usr-overlay，装系统包务必走 deb，
  不要手动 cp（upperdir 与 overlay 语义不同，会制造 bind mount 类残留）。

### 5.7 打开文件/目录与 CI 发布（2026-08 实测）
- **QDesktopServices::openUrl 在 deepin 上走 xdg-desktop-portal，portal 失效时返回 true 但不开窗口**
  （`gio open` 同样失败）；`xdg-open` 在控制中心进程环境也一样。**绕过方案**：
  打开目录用 `QProcess::startDetached("dde-file-manager", {dir})`；打开文件需系统装好
  PDF 查看器（如 deepin-reader），否则默认应用指向空 → 无窗口但返回 OK。
- **arm64 链接必须 -fPIC**：静态库链进插件 .so 时，AArch64 报 `dangerous relocation`、
  x86_64 侥幸通过——`src/plugin/CMakeLists.txt` 已全局 `set(CMAKE_POSITION_INDEPENDENT_CODE ON)`。
- **多架构 CI**：`.github/workflows/build-deb.yml` 用 debootstrap deepin beige rootfs 隔离构建
  （loong64 无原生 runner，`--foreign` + qemu-user-static 二阶段）；打包用
  `make install DESTDIR=/tmp/inst` + `ci/package-deb.sh`（插件路径由 CMake 决定，不硬编码 v1.0/v1.1）。
- **gh release upload --clobber 有 404 bug**：上传 Release 资产用手动 API
  （GET release → DELETE 同名 assets/{id} → POST uploads.github.com）；`gh api --input` 上传文件。
- **只推文档不想触发 CI**：commit message 加 `[skip ci]`（GitHub Actions 跳过该提交）；
  上传 Release 资产（API）本身不触发 workflow。

## 6. 二次开发常见任务指引

| 想做什么 | 改哪里 |
| --- | --- |
| 加一个设置项 | `src/service/configmanager.{h,cpp}`（键/读写）+ `PdfprinterSettingsPage.qml` + backend 读取 |
| 改文件列表展示 | `PdfprinterFilesPage.qml` + `pdfFileDetails`（`pdfprintermodule.cpp`） |
| 新增页面 | 新建 `PdfprinterXxxPage.qml` → `PdfprinterMain.qml` 注册（weight 递增） |
| 改打印机行为 | `src/service/printermanager.cpp`（QProcess 调 lpadmin/lpstat） |
| 改输出文件名规则 | `backend/ddepdf`（sanitize_filename + 命名模板） |

**修改后必做**：
1. `rm -rf build/integration && cmake ... && make`（QML 变更时必须）
2. `lupdate qml/ -ts translations/pdfprinter_zh_CN.ts` + 去掉 unfinished 标记 + `lrelease`（翻译变更时）
3. 重新打包 deb → dpkg -i → 控制中心实测

## 7. 验证清单（改动自测）

```bash
# 静态
/usr/lib/qt6/bin/qmllint src/plugin/qml/*.qml     # QML 语法
python3 -m py_compile backend/ddepdf            # backend 语法

# 运行时（控制中心 D-Bus）
dbus-send --session --dest=org.deepin.dde.ControlCenter1 --type=method_call --print-reply \
  /org/deepin/dde/ControlCenter1 org.deepin.dde.ControlCenter1.GetAllModule   # 模块树
dbus-send --session --dest=org.deepin.dde.ControlCenter1 --type=method_call --print-reply \
  /org/deepin/dde/ControlCenter1 org.deepin.dde.ControlCenter1.ShowPage string:pdfprinter/<page>

# 端到端
lp -d DDE-PDF /path/to/file && ls -t ~/PDF/ | head -1   # 打印 → PDF
```

## 8. 安全红线（评审/参赛要求）

- ❌ 无任何网络请求（socket/curl/http）——功能纯本地
- ❌ 不读 /etc/passwd、/proc、用户文档（除输出目录内 *.pdf）
- ✅ 命令执行白名单：仅 lpadmin / lpstat / su(mkdir)
- ✅ 文件写入仅限用户配置的输出目录
- 新增代码保持同等约束，否则评审不过

## 8.5 命名审查（防误导 deepin 官方，必做）

**对外可见的名称一律不得包含 "deepin"**（防止用户误以为是 deepin 官方出品）：

| 范围 | 要求 |
| --- | --- |
| 作品名 / 仓库名 | ✅ 用 DDE 生态表述（如「DDE 虚拟 PDF 打印机」/ `dde-virtual-pdf-printer`） |
| deb 包名 / 可执行名 / 应用显示名 | ❌ 不得含 `deepin`（如 `dde-pdf-printer`、`deepinpdf` 均违规） |
| 帮助页版本号 / README 标题 | ❌ 不得含 `deepin`（版本展示如 `dde-pdf-printer vX.Y.Z`） |
| 插件显示名 | ✅ 「PDF 打印机」（模块名 `pdfprinter` 无 deepin 字样） |

**允许保留**（技术事实描述，非名称宣传）：
- 运行平台表述（"在 deepin 上运行"）
- 系统服务名（`com.deepin.filemanager.filedialog`）、系统路径（`/usr/share/dsg`）
- 配置命名空间 `org.deepin.dde.pdfprinter`（反向域名惯例，非作品名）

**发布前检查**：
```bash
# 对外名称扫描（包名/显示名/标题/版本串，排除技术引用）
grep -rniE 'deepin-(pdf|printer)|deepinpdf' debian/ src/ README.md AGENTS.md docs/contest-post.md
# 结果必须为空或仅注释/技术描述
```

## 9. 许可证与参赛

- GPL-3.0-or-later；基于 deepin Skills 开发，参赛作品
- 文档：`docs/design.md`（架构）、`docs/security-audit.md`（安全审查）、`docs/forum-print-survey.md`（需求调研）

### 5.8 .desktop 唤起控制中心（2026-08 实测）
- **`dde-control-center --show` 是官方唤起参数，一行搞定**：未运行 → 启动并显示窗口；
  已运行+隐藏 → 唤回窗口（单实例自动处理）。
  ```ini
  Exec=dde-control-center --show
  ```
- 命令行不带参数启动只注册 D-Bus 服务不显示窗口（表现为「有时候唤不起来」）。
- 若需直达插件页（可选）：`--show` 启动后 `dbus-send ... ShowModule string:pdfprinter`。

