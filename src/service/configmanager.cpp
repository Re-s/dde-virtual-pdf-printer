#include "configmanager.h"

#include <QDir>
#include <QSettings>

namespace {
const QString kOrgName = QStringLiteral("org.deepin.dde.pdfprinter");
const QString kAppName = QStringLiteral("pdfprinter");
const QString kKeyOutputDir = QStringLiteral("outputDir");
const QString kKeyAutoOpen = QStringLiteral("autoOpen");
const QString kKeyFilenameTemplate = QStringLiteral("filenameTemplate");
const QString kDefaultTemplate = QStringLiteral("{title}-{jobid}-{date}-{time}");

QSettings openSettings()
{
    return QSettings(kOrgName, kAppName);
}
} // namespace

ConfigManager::ConfigManager(QObject *parent)
    : QObject(parent)
{
}

QString ConfigManager::outputDir() const
{
    const QSettings settings = openSettings();
    const QString dir = settings.value(kKeyOutputDir).toString().trimmed();
    if (dir.isEmpty()) {
        return QDir::homePath() + QStringLiteral("/PDF");
    }
    return dir;
}

void ConfigManager::setOutputDir(const QString &dir)
{
    if (dir == outputDir()) {
        return;
    }
    QSettings settings = openSettings();
    settings.setValue(kKeyOutputDir, dir);
    syncConfig();
    emit outputDirChanged();
}

bool ConfigManager::autoOpen() const
{
    const QSettings settings = openSettings();
    return settings.value(kKeyAutoOpen, false).toBool();
}

void ConfigManager::setAutoOpen(bool open)
{
    if (open == autoOpen()) {
        return;
    }
    QSettings settings = openSettings();
    settings.setValue(kKeyAutoOpen, open);
    syncConfig();
    emit autoOpenChanged();
}

QString ConfigManager::filenameTemplate() const
{
    const QSettings settings = openSettings();
    const QString tpl = settings.value(kKeyFilenameTemplate).toString().trimmed();
    if (tpl.isEmpty()) {
        return kDefaultTemplate;
    }
    return tpl;
}

void ConfigManager::setFilenameTemplate(const QString &tpl)
{
    const QString clean = tpl.trimmed();
    if (clean == filenameTemplate()) {
        return;
    }
    QSettings settings = openSettings();
    settings.setValue(kKeyFilenameTemplate, clean);
    syncConfig();
    emit filenameTemplateChanged();
}

void ConfigManager::syncConfig()
{
    QSettings settings = openSettings();
    settings.sync();
}
