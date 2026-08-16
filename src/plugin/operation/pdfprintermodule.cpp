// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later
#include "pdfprintermodule.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusObjectPath>
#include <QDBusReply>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QDebug>
#include <QStandardPaths>
#include <QTextStream>
#include <QUrl>
#include <QVariantList>
#include <QVariantMap>

#include <algorithm>

#include "../../service/logger.h"
#include "../../service/printermanager.h"
#include "../../service/configmanager.h"
#include "../../service/outputdirwatcher.h"

#include "dccfactory.h"

namespace {

// 插件版本（帮助页展示 + 日志记录，与 deb 包 Version 保持一致）
const QString kPluginVersion = QStringLiteral("0.8.6");

// debug 模式日志：每个 Q_INVOKABLE 功能调用都会写入日志文件，方便排查"按钮点了没反应"。
// 写文件实现已提取到 src/service/logger.h（与 service 层共享）。

void logCall(const QString &feature, const QString &detail = {})
{
    const QString msg = QStringLiteral("[pdfprinter] CALL %1%2")
                            .arg(feature,
                                 detail.isEmpty() ? QString() : QStringLiteral(" | ") + detail);
    pdfprinterWriteLog(msg);
    qDebug().noquote() << msg;  // 保险：能进 journal 则双通道
}

void logResult(const QString &feature, bool ok)
{
    const QString msg = QStringLiteral("[pdfprinter] RESULT %1 -> %2")
                            .arg(feature, ok ? QStringLiteral("OK") : QStringLiteral("FAIL"));
    pdfprinterWriteLog(msg);
    qDebug().noquote() << msg;
}

// Human-readable file size: 523 B, 12.5 KB, 3.42 MB, ...
QString formatFileSize(qint64 bytes)
{
    if (bytes < 1024) {
        return QStringLiteral("%1 B").arg(bytes);
    }
    if (bytes < 1024 * 1024) {
        return QStringLiteral("%1 KB").arg(QString::number(bytes / 1024.0, 'f', 1));
    }
    if (bytes < 1024LL * 1024 * 1024) {
        return QStringLiteral("%1 MB").arg(QString::number(bytes / (1024.0 * 1024.0), 'f', 2));
    }
    return QStringLiteral("%1 GB").arg(QString::number(bytes / (1024.0 * 1024.0 * 1024.0), 'f', 2));
}

} // namespace

// PIMPL: compose the service layer. Impl is parented to the module itself,
// and every service object is parented to Impl, so all child objects created
// in the constructor share the module's thread affinity (DccFactory::create()
// may run on a worker thread; parentless QObjects would leak / misbehave).
class PdfPrinterModule::Impl : public QObject
{
public:
    explicit Impl(PdfPrinterModule *q)
        : QObject(q)
        , configManager(new ConfigManager(this))
        , printerManager(new PrinterManager(configManager, this))
        , watcher(new OutputDirWatcher(this))
    {
        // 插件加载即记录版本（排查问题时先确认版本）
        pdfprinterWriteLog(QStringLiteral("[pdfprinter] VERSION %1 (plugin loaded)").arg(kPluginVersion));
        // Relay printer state changes to the module.
        connect(printerManager, &PrinterManager::printerStateChanged,
                q, &PdfPrinterModule::printerStateChanged);
        // Keep the pdf file cache in sync whenever the service notices changes.
        connect(printerManager, &PrinterManager::pdfFilesChanged,
                q, &PdfPrinterModule::refreshPdfList);
        // Relay config changes and re-arm the watcher for the new directory.
        connect(configManager, &ConfigManager::outputDirChanged, this, [this, q]() {
            watcher->watch(configManager->outputDir());
            Q_EMIT q->outputDirChanged();
            q->refreshPdfList();
        });
        connect(configManager, &ConfigManager::autoOpenChanged,
                q, &PdfPrinterModule::autoOpenChanged);
        connect(configManager, &ConfigManager::filenameTemplateChanged,
                q, &PdfPrinterModule::filenameTemplateChanged);
        connect(configManager, &ConfigManager::keepTitleExtensionChanged,
                q, &PdfPrinterModule::keepTitleExtensionChanged);
        // Directory content changed (new PDF printed, file removed, ...).
        connect(watcher, &OutputDirWatcher::filesChanged,
                q, &PdfPrinterModule::refreshPdfList);

        // NOTE: cannot call q->refreshPdfList() here -- q->d is not yet
        // assigned during Impl construction. Also avoid calling
        // printerManager->listPdfFiles() or configManager->outputDir() here
        // because this constructor runs in DccFactory's thread pool and
        // ConfigManager may not be safe to access from worker threads.
        // Initialize empty caches; they will be populated by the first
        // refreshPdfList() call after the module is fully constructed.
    }

    PrinterManager *printerManager;
    ConfigManager *configManager;
    OutputDirWatcher *watcher;

    QStringList pdfFiles;   // cached file name list, refreshed by refreshPdfList()
    QStringList lastFiles;  // previous snapshot used to diff newly added files
    QString lastError;
    QString dialogPath;     // 当前打开的 D-Bus 文件对话框路径（空 = 无）
    bool busy = false;
};

PdfPrinterModule::PdfPrinterModule(QObject *parent)
    : QObject(parent)
    , d(new Impl(this))
{
    // Initialize file list and start watching after module is fully constructed.
    // These must be called here (not in Impl constructor) because
    // DccFactory::create() runs in a thread pool and ConfigManager
    // may not be safe to access during construction.
    refreshPdfList();
    d->watcher->watch(d->configManager->outputDir());
}

QString PdfPrinterModule::pluginVersion() const
{
    return kPluginVersion;
}

bool PdfPrinterModule::printerExists() const
{
    return d->printerManager->printerExists();
}

QString PdfPrinterModule::printerName() const
{
    return d->printerManager->printerName();
}

QString PdfPrinterModule::outputDir() const
{
    return d->configManager->outputDir();
}

void PdfPrinterModule::setOutputDir(const QString &dir)
{
    d->configManager->setOutputDir(dir);
}

bool PdfPrinterModule::autoOpen() const
{
    return d->configManager->autoOpen();
}

QString PdfPrinterModule::filenameTemplate() const
{
    return d->configManager->filenameTemplate();
}

void PdfPrinterModule::setFilenameTemplate(const QString &tpl)
{
    d->configManager->setFilenameTemplate(tpl);
}

bool PdfPrinterModule::keepTitleExtension() const
{
    return d->configManager->keepTitleExtension();
}

void PdfPrinterModule::setKeepTitleExtension(bool keep)
{
    d->configManager->setKeepTitleExtension(keep);
}

void PdfPrinterModule::setAutoOpen(bool open)
{
    d->configManager->setAutoOpen(open);
}

QStringList PdfPrinterModule::pdfFiles() const
{
    return d->pdfFiles;
}

bool PdfPrinterModule::busy() const
{
    return d->busy;
}

QString PdfPrinterModule::lastError() const
{
    return d->lastError;
}

bool PdfPrinterModule::createPrinter()
{
    logCall(QStringLiteral("createPrinter"));
    if (!d->busy) {
        d->busy = true;
        Q_EMIT busyChanged();
    }
    const bool ok = d->printerManager->createPrinter();
    if (d->busy) {
        d->busy = false;
        Q_EMIT busyChanged();
    }
    if (ok) {
        d->lastError.clear();
        Q_EMIT lastErrorChanged();
        refreshPdfList();
    } else {
        d->lastError = QStringLiteral("create failed: 打印机创建失败，请检查 CUPS 服务状态与用户权限");
        Q_EMIT lastErrorChanged();
    }
    return ok;
}

bool PdfPrinterModule::removePrinter()
{
    logCall(QStringLiteral("removePrinter"));
    if (!d->busy) {
        d->busy = true;
        Q_EMIT busyChanged();
    }
    const bool ok = d->printerManager->removePrinter();
    if (d->busy) {
        d->busy = false;
        Q_EMIT busyChanged();
    }
    if (ok) {
        d->lastError.clear();
        Q_EMIT lastErrorChanged();
        refreshPdfList();
    } else {
        d->lastError = QStringLiteral("remove failed: 打印机删除失败，请检查用户权限");
        Q_EMIT lastErrorChanged();
    }
    return ok;
}

void PdfPrinterModule::refreshPdfList()
{
    logCall(QStringLiteral("refreshPdfList"));
    const QStringList files = d->printerManager->listPdfFiles();

    // Diff against the previous snapshot to find newly added PDFs (name-set
    // difference), so autoOpen never re-opens files on plain refreshes.
    QStringList newFiles;
    for (const QString &file : files) {
        if (!d->lastFiles.contains(file)) {
            newFiles.append(file);
        }
    }
    d->lastFiles = files;

    if (files != d->pdfFiles) {
        d->pdfFiles = files;
        Q_EMIT pdfFilesChanged();
    }

    // Auto-open only genuinely new files when the feature is enabled.
    // 统一走 PrinterManager::openPdfFile（官方 dde-open，绕 portal 失效问题）
    if (d->configManager->autoOpen() && !newFiles.isEmpty()) {
        const QDir dir(d->configManager->outputDir());
        for (const QString &file : newFiles) {
            if (d->printerManager->openPdfFile(file)) {
                Q_EMIT pdfAutoOpened(dir.filePath(file));
            }
        }
    }
}

QVariantList PdfPrinterModule::pdfFileDetails() const
{
    // Reuse PrinterManager::listPdfFiles() for the directory scan and sort
    // instead of duplicating the same logic here.
    const QStringList names = d->printerManager->listPdfFiles();
    if (names.isEmpty()) {
        return {};
    }
    const QDir dir(d->configManager->outputDir());
    QVariantList details;
    details.reserve(names.size());
    for (const QString &name : names) {
        const QFileInfo fi(dir.filePath(name));
        QVariantMap map;
        map.insert(QStringLiteral("name"), name);
        map.insert(QStringLiteral("size"), fi.size());
        map.insert(QStringLiteral("sizeText"), formatFileSize(fi.size()));
        map.insert(QStringLiteral("mtimeText"),
                   fi.lastModified().toString(QStringLiteral("yyyy-MM-dd hh:mm")));
        map.insert(QStringLiteral("path"), fi.absoluteFilePath());
        details.append(map);
    }
    return details;
}

bool PdfPrinterModule::openOutputDir()
{
    logCall(QStringLiteral("openOutputDir"), d->printerManager->outputDir());
    const bool ok = d->printerManager->openOutputDir();
    logResult(QStringLiteral("openOutputDir"), ok);
    if (!ok) {
        d->lastError = QStringLiteral("open dir failed: 无法打开输出目录");
        Q_EMIT lastErrorChanged();
    }
    return ok;
}

bool PdfPrinterModule::openPdfFile(int index)
{
    logCall(QStringLiteral("openPdfFile"), QStringLiteral("index=%1").arg(index));
    if (index < 0 || index >= d->pdfFiles.size()) {
        d->lastError = QStringLiteral("invalid index: 文件索引超出范围");
        Q_EMIT lastErrorChanged();
        return false;
    }
    const bool ok = d->printerManager->openPdfFile(d->pdfFiles.at(index));
    logResult(QStringLiteral("openPdfFile"), ok);
    if (!ok) {
        d->lastError = QStringLiteral("open failed: 无法打开 PDF 文件");
        Q_EMIT lastErrorChanged();
    }
    return ok;
}

bool PdfPrinterModule::deletePdfFile(int index)
{
    logCall(QStringLiteral("deletePdfFile"), QStringLiteral("index=%1").arg(index));
    if (index < 0 || index >= d->pdfFiles.size()) {
        d->lastError = QStringLiteral("invalid index: 文件索引超出范围");
        Q_EMIT lastErrorChanged();
        return false;
    }
    const bool ok = d->printerManager->deletePdfFile(d->pdfFiles.at(index));
    if (ok) {
        d->lastError.clear();
        Q_EMIT lastErrorChanged();
        refreshPdfList();
    } else {
        d->lastError = QStringLiteral("delete failed: 无法删除 PDF 文件");
        Q_EMIT lastErrorChanged();
    }
    return ok;
}

QString PdfPrinterModule::defaultOutputDir()
{
    return QDir::homePath() + QStringLiteral("/PDF");
}

void PdfPrinterModule::openOutputDirPicker()
{
    logCall(QStringLiteral("openOutputDirPicker"));
    // dde-control-center 是纯 QML 应用（QGuiApplication），不能使用
    // QFileDialog（QtWidgets 模块）——会因无 QApplication 直接崩溃。
    // 方案：通过 D-Bus 调用 deepin 官方文件对话框服务
    // com.deepin.filemanager.filedialog 的目录选择模式。
    // 异步实现：show() 后立即返回，selectedUrls/rejected 信号通过
    // onDialogUrls/onDialogRejected 回调，结果经 outputDirPicked 发出。
    // （不能用 QEventLoop 阻塞——控制中心主线程被占会导致死锁）
    QDBusConnection bus = QDBusConnection::sessionBus();
    const QString managerService = QStringLiteral("com.deepin.filemanager.filedialog");
    const QString managerPath = QStringLiteral("/com/deepin/filemanager/filedialogmanager");

    // 若已有对话框打开，先关闭旧的
    if (!d->dialogPath.isEmpty()) {
        QDBusInterface oldDlg(managerService, d->dialogPath,
                              QStringLiteral("com.deepin.filemanager.filedialog"), bus);
        oldDlg.call(QStringLiteral("reject"));
        d->dialogPath.clear();
    }

    // 1. 创建对话框
    QDBusInterface manager(managerService, managerPath,
                           QStringLiteral("com.deepin.filemanager.filedialogmanager"), bus);
    if (!manager.isValid()) {
        d->lastError = QStringLiteral("文件对话框服务不可用");
        Q_EMIT lastErrorChanged();
        return;
    }
    // key 必须是纯字母数字（dde-file-dialog 对象路径限制，连字符会导致 NoReply）
    const QString key = QStringLiteral("pdfprinter%1").arg(QCoreApplication::applicationPid());
    QDBusReply<QDBusObjectPath> reply = manager.call(QStringLiteral("createDialog"), key);
    if (!reply.isValid()) {
        d->lastError = QStringLiteral("创建文件对话框失败: %1").arg(reply.error().message());
        Q_EMIT lastErrorChanged();
        return;
    }
    const QString dialogPath = reply.value().path();

    // 2. 连接信号（不阻塞；信号到达时通过槽回调）
    // 注意：selectedUrls 是方法不是信号！真正的信号是 accepted/rejected，
    // accepted 后需调用 selectedUrls 方法获取结果。
    bus.connect(managerService, dialogPath,
                QStringLiteral("com.deepin.filemanager.filedialog"),
                QStringLiteral("accepted"),
                this, SLOT(onDialogAccepted()));
    bus.connect(managerService, dialogPath,
                QStringLiteral("com.deepin.filemanager.filedialog"),
                QStringLiteral("rejected"),
                this, SLOT(onDialogRejected()));

    // 3. 配置为目录选择模式并显示
    QDBusInterface dlg(managerService, dialogPath,
                       QStringLiteral("com.deepin.filemanager.filedialog"), bus);
    if (!dlg.isValid()) {
        manager.call(QStringLiteral("destroyDialog"), QVariant::fromValue(QDBusObjectPath(dialogPath)));
        return;
    }
    dlg.setProperty("acceptMode", QVariant::fromValue(0));        // AcceptOpen
    dlg.call(QStringLiteral("setFileMode"), 2);                    // QFileDialog::Directory
    dlg.call(QStringLiteral("setWindowTitle"), QStringLiteral("选择 PDF 输出目录"));
    dlg.setProperty("directoryUrl",
                    QVariant::fromValue(QUrl::fromLocalFile(d->configManager->outputDir()).toString()));
    d->dialogPath = dialogPath;
    dlg.call(QStringLiteral("show"));
}

void PdfPrinterModule::onDialogAccepted()
{
    // 用户确认选择（accepted 信号）——调用 selectedUrls 方法获取结果
    QString chosen;
    const QString managerService = QStringLiteral("com.deepin.filemanager.filedialog");
    if (!d->dialogPath.isEmpty()) {
        QDBusInterface dlg(managerService, d->dialogPath,
                           QStringLiteral("com.deepin.filemanager.filedialog"),
                           QDBusConnection::sessionBus());
        QDBusReply<QStringList> urls = dlg.call(QStringLiteral("selectedUrls"));
        if (urls.isValid() && !urls.value().isEmpty()) {
            chosen = QUrl(urls.value().first()).toLocalFile();
        }
    }
    cleanupDialog();
    Q_EMIT outputDirPicked(chosen);
}

void PdfPrinterModule::onDialogRejected()
{
    // 用户取消（rejected 信号）
    cleanupDialog();
    Q_EMIT outputDirPicked(QString());
}

void PdfPrinterModule::cleanupDialog()
{
    if (d->dialogPath.isEmpty()) {
        return;
    }
    const QString managerService = QStringLiteral("com.deepin.filemanager.filedialog");
    const QString managerPath = QStringLiteral("/com/deepin/filemanager/filedialogmanager");
    QDBusConnection bus = QDBusConnection::sessionBus();

    bus.disconnect(managerService, d->dialogPath,
                   QStringLiteral("com.deepin.filemanager.filedialog"),
                   QStringLiteral("accepted"),
                   this, SLOT(onDialogAccepted()));
    bus.disconnect(managerService, d->dialogPath,
                   QStringLiteral("com.deepin.filemanager.filedialog"),
                   QStringLiteral("rejected"),
                   this, SLOT(onDialogRejected()));

    QDBusInterface manager(managerService, managerPath,
                           QStringLiteral("com.deepin.filemanager.filedialogmanager"), bus);
    manager.call(QStringLiteral("destroyDialog"), QVariant::fromValue(QDBusObjectPath(d->dialogPath)));
    d->dialogPath.clear();
}

// Register the plugin factory (dccfactory.h); generates PdfPrinterModuleDccFactory.
DCC_FACTORY_CLASS(PdfPrinterModule)
#include "pdfprintermodule.moc"
