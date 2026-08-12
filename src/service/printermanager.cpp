#include "printermanager.h"
#include "configmanager.h"

#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QProcessEnvironment>
#include <QRegularExpression>
#include <QUrl>

#include <algorithm>

namespace {

const QString kPrinterName = QStringLiteral("Deepin-PDF");

// Successful `lpstat -p <name>` (LC_ALL=C) prints a line starting with
// "printer <name> ...". A *missing* printer prints an error on stderr such as
//   lpstat: Invalid destination name in list "Deepin-PDF".
// which also contains the printer name, so a naive contains() match would
// keep reporting a removed printer as existing. Anchor the match to the
// "printer <name>" prefix instead.
const QRegularExpression kPrinterLine(QStringLiteral("^printer %1\\b")
        .arg(QRegularExpression::escape(kPrinterName)));

// Run lpstat with LC_ALL=C so the output is stable English text, e.g.
// "printer Deepin-PDF is idle. enabled since ..." (localized output would
// otherwise break keyword matching). Returns combined stdout+stderr.
QString runLpstat(const QStringList &args)
{
    QProcess proc;
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert(QStringLiteral("LC_ALL"), QStringLiteral("C"));
    proc.setProcessEnvironment(env);
    proc.start(QStringLiteral("lpstat"), args);
    if (!proc.waitForStarted(5000) || !proc.waitForFinished(10000)) {
        return {};
    }
    return QString::fromLocal8Bit(proc.readAllStandardOutput() + proc.readAllStandardError());
}

} // namespace

PrinterManager::PrinterManager(ConfigManager *config, QObject *parent)
    : QObject(parent)
    , m_config(config)
{
}

bool PrinterManager::printerExists() const
{
    return kPrinterLine.match(runLpstat({ QStringLiteral("-p"), kPrinterName })).hasMatch();
}

bool PrinterManager::isEnabled() const
{
    const QString out = runLpstat({ QStringLiteral("-p"), kPrinterName });
    return kPrinterLine.match(out).hasMatch() && out.contains(QStringLiteral("enabled"));
}

QString PrinterManager::printerName() const
{
    return kPrinterName;
}

QString PrinterManager::outputDir() const
{
    // 统一从配置读取输出目录（控制中心修改后全局生效）
    if (m_config) {
        return m_config->outputDir();
    }
    return QDir::homePath() + QStringLiteral("/PDF");
}

bool PrinterManager::createPrinter()
{
    const bool ok = runCommand(QStringLiteral("lpadmin"), {
        QStringLiteral("-p"), kPrinterName,
        QStringLiteral("-E"),
        QStringLiteral("-v"), QStringLiteral("deepinpdf:/"),
        QStringLiteral("-P"), QStringLiteral("/usr/share/ppd/cupsfilters/Generic-PDF_Printer-PDF.ppd"),
    });
    if (ok) {
        emit printerStateChanged();
    }
    return ok;
}

bool PrinterManager::removePrinter()
{
    const bool ok = runCommand(QStringLiteral("lpadmin"), { QStringLiteral("-x"), kPrinterName });
    if (ok) {
        emit printerStateChanged();
    }
    return ok;
}

bool PrinterManager::openOutputDir()
{
    return QDesktopServices::openUrl(QUrl::fromLocalFile(outputDir()));
}

QStringList PrinterManager::listPdfFiles() const
{
    const QString dirPath = outputDir();
    QDir dir(dirPath);
    if (!dir.exists()) {
        return {};
    }
    const QStringList names = dir.entryList({ QStringLiteral("*.pdf") }, QDir::Files, QDir::Name);
    // Sort by last-modified time, newest first (stable for equal timestamps).
    QStringList sorted = names;
    std::sort(sorted.begin(), sorted.end(), [&dir](const QString &a, const QString &b) {
        return QFileInfo(dir.filePath(a)).lastModified() > QFileInfo(dir.filePath(b)).lastModified();
    });
    return sorted;
}

bool PrinterManager::openPdfFile(const QString &fileName) const
{
    return QDesktopServices::openUrl(QUrl::fromLocalFile(QDir(outputDir()).filePath(fileName)));
}

bool PrinterManager::deletePdfFile(const QString &fileName) const
{
    if (!QFile::remove(QDir(outputDir()).filePath(fileName))) {
        return false;
    }
    // Signals are non-const member functions; the contract pins this method as
    // const, so emit through a const_cast to notify listeners of the change.
    const_cast<PrinterManager *>(this)->pdfFilesChanged();
    return true;
}

bool PrinterManager::runCommand(const QString &cmd, QStringList args) const
{
    QProcess proc;
    proc.start(cmd, args);
    if (!proc.waitForStarted(5000)) {
        return false;
    }
    if (!proc.waitForFinished(30000)) {
        return false;
    }
    // Note: lpadmin may print deprecation warnings on stderr; they do not
    // affect the exit code, so only the exit status/code decides success.
    return proc.exitStatus() == QProcess::NormalExit && proc.exitCode() == 0;
}
