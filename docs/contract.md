# deepin-pdf-printer 模块化开发契约

> 本文档是并行开发的任务边界定义。三个子任务**严格按此契约**实现，
> 接口签名不得改动。集成时由主 agent 统一构建验证。

## 任务划分

| 任务 | 目录 | 内容 | 依赖 |
|------|------|------|------|
| A 服务层 | `src/service/` | PrinterManager、ConfigManager、OutputDirWatcher | 无（独立） |
| B 插件C++ | `src/plugin/operation/` | PdfPrinterModule（DCC_FACTORY_CLASS）、PdfFileModel | A 的头文件 |
| C 插件QML | `src/plugin/qml/` | 4 个页面 + 根对象 | B 暴露的 dccData 接口 |

## 项目根目录

`/home/master/Projects/deepin-pdf-printer/`

## 接口契约（钉死，不得修改签名）

### A. src/service/printermanager.h（Task A 实现 .cpp，Task B 引用）

```cpp
#pragma once
#include <QObject>
#include <QStringList>

class PrinterManager : public QObject
{
    Q_OBJECT
public:
    explicit PrinterManager(QObject *parent = nullptr);

    // 状态查询
    bool printerExists() const;        // lpstat 检测 Deepin-PDF 打印机
    bool isEnabled() const;            // 打印机是否启用
    QString printerName() const;       // 固定返回 "Deepin-PDF"
    QString outputDir() const;         // 默认 ~/PDF

    // 操作（返回是否成功）
    bool createPrinter();              // lpadmin -p Deepin-PDF -E -v deepinpdf:/ -P Generic-PDF PPD
    bool removePrinter();              // lpadmin -x Deepin-PDF
    bool openOutputDir();              // 文件管理器打开输出目录

    // PDF 文件
    QStringList listPdfFiles() const;  // 输出目录 *.pdf 文件名列表（按修改时间倒序）
    bool openPdfFile(const QString &fileName) const;   // 用默认应用打开
    bool deletePdfFile(const QString &fileName) const; // 删除指定 PDF

Q_SIGNALS:
    void printerStateChanged();        // 打印机创建/删除后发出
    void pdfFilesChanged();            // 文件列表变化后发出

private:
    bool runCommand(const QString &cmd, QStringList args) const;
};
```

### A. src/service/configmanager.h（Task A 实现 .cpp，Task B 引用）

```cpp
#pragma once
#include <QObject>
#include <QString>

class ConfigManager : public QObject
{
    Q_OBJECT
public:
    explicit ConfigManager(QObject *parent = nullptr);

    QString outputDir() const;         // 自定义输出目录（默认 ~/PDF）
    void setOutputDir(const QString &dir);
    bool autoOpen() const;             // 打印后自动打开 PDF
    void setAutoOpen(bool open);

Q_SIGNALS:
    void outputDirChanged();
    void autoOpenChanged();

private:
    void syncConfig();                 // 写回 DConfig
};
```

### A. src/service/outputdirwatcher.h（Task A 实现 .cpp，Task B 引用）

```cpp
#pragma once
#include <QObject>
#include <QFileSystemWatcher>

class OutputDirWatcher : public QObject
{
    Q_OBJECT
public:
    explicit OutputDirWatcher(QObject *parent = nullptr);
    void watch(const QString &dir);    // 监听目录变化

Q_SIGNALS:
    void filesChanged();               // 目录新增/删除文件

private:
    QFileSystemWatcher m_watcher;
};
```

### B. src/plugin/operation/pdfprintermodule.h（Task B 实现，Task C 按此写 QML）

```cpp
#pragma once
#include <QObject>
#include <QStringList>

class PdfPrinterModule : public QObject
{
    Q_OBJECT
    // QML 可读属性
    Q_PROPERTY(bool printerExists READ printerExists NOTIFY printerStateChanged FINAL)
    Q_PROPERTY(QString printerName READ printerName CONSTANT FINAL)
    Q_PROPERTY(QString outputDir READ outputDir WRITE setOutputDir NOTIFY outputDirChanged FINAL)
    Q_PROPERTY(bool autoOpen READ autoOpen WRITE setAutoOpen NOTIFY autoOpenChanged FINAL)
    Q_PROPERTY(QStringList pdfFiles READ pdfFiles NOTIFY pdfFilesChanged FINAL)
    Q_PROPERTY(QVariantList pdfFileDetails READ pdfFileDetails NOTIFY pdfFilesChanged FINAL)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged FINAL)
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged FINAL)
public:
    explicit PdfPrinterModule(QObject *parent = nullptr);

    bool printerExists() const;
    QString printerName() const;
    QString outputDir() const;
    void setOutputDir(const QString &dir);
    bool autoOpen() const;
    void setAutoOpen(bool open);
    QStringList pdfFiles() const;
    QVariantList pdfFileDetails() const;   // [{name,size,sizeText,mtimeText,path}, ...] 按时间倒序
    bool busy() const;
    QString lastError() const;

    // QML 可调用方法
    Q_INVOKABLE bool createPrinter();
    Q_INVOKABLE bool removePrinter();
    Q_INVOKABLE void refreshPdfList();
    Q_INVOKABLE bool openOutputDir();
    Q_INVOKABLE bool openPdfFile(int index);
    Q_INVOKABLE bool deletePdfFile(int index);
    Q_INVOKABLE QString defaultOutputDir();
    Q_INVOKABLE QString chooseOutputDir();  // 弹出目录选择对话框，返回选择的目录（取消返回空串）

Q_SIGNALS:
    void printerStateChanged();
    void outputDirChanged();
    void autoOpenChanged();
    void pdfFilesChanged();
    void busyChanged();
    void lastErrorChanged();
    void pdfAutoOpened(const QString &filePath);  // autoOpen 生效时发出

private:
    class Impl;  // PIMPL：组合 PrinterManager/ConfigManager/OutputDirWatcher
    Impl *d;
};
```

## 环境要点（所有任务必须遵守）

1. **cmake 用 `/usr/local/bin/cmake`**（wrapper，自动处理 LD_LIBRARY_PATH 污染）
2. **DTK/Qt 版本**：控制中心插件用 Qt6 + Dtk6（`find_package(Dtk6Widget)`，target `Dtk6::Widget`）；本机有 libdtk6*-dev 6.7.47
3. **编译命令**：`cd <build> && /usr/local/bin/cmake .. && make -j$(nproc)`
4. 参考官方示例：`/home/master/Projects/refs/dde-control-center/examples/plugin-example/`
5. 插件命名：PLUGIN_NAME = `pdfprinter`；根 QML name = `pdfprinter`；QML 文件首字母大写
6. DCC 插件 CMake 依赖：`find_package(DdeControlCenter REQUIRED)`（已装 dde-control-center-dev 6.1.101）
7. 网络请求用 `python3 /home/master/.local/bin/netfetch.py fetch <url>`（自动直连/代理）

## 验证要求

- Task A：服务层必须能独立编译通过（可写一个临时 main.cpp 冒烟测试）
- Task B：插件 C++ 必须能编译（可先只编译 operation/ 的 .cpp 检查语法）
- Task C：QML 文件通过 `qmlformat` 或至少语法检查
- 所有任务**不要**运行 GUI（需要 TMPDIR=/tmp 且当前会话可能无显示权限）
