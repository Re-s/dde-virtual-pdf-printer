#include "configmanager.h"

#include <QDir>
#include <QSettings>

namespace {
const QString kOrgName = QStringLiteral("org.deepin.dde.pdfprinter");
const QString kAppName = QStringLiteral("pdfprinter");
const QString kKeyOutputDir = QStringLiteral("outputDir");
const QString kKeyAutoOpen = QStringLiteral("autoOpen");
const QString kKeyFilenameTemplate = QStringLiteral("filenameTemplate");
const QString kKeyKeepTitleExtension = QStringLiteral("keepTitleExtension");
const QString kDefaultTemplate = QStringLiteral("{title}-{jobid}-{date}-{time}");

QSettings openSettings()
{
    return QSettings(kOrgName, kAppName);
}
} // namespace

ConfigManager::ConfigManager(QObject *parent)
    : QObject(parent)
{
    invalidateCache();
}

QString ConfigManager::outputDir() const
{
    if (!m_cacheValid) {
        const QSettings settings = openSettings();
        const QString dir = settings.value(kKeyOutputDir).toString().trimmed();
        m_outputDir = dir.isEmpty() ? QDir::homePath() + QStringLiteral("/PDF") : dir;
        m_autoOpen = settings.value(kKeyAutoOpen, false).toBool();
        const QString tpl = settings.value(kKeyFilenameTemplate).toString().trimmed();
        m_filenameTemplate = tpl.isEmpty() ? kDefaultTemplate : tpl;
        m_keepTitleExtension = settings.value(kKeyKeepTitleExtension, false).toBool();
        m_cacheValid = true;
    }
    return m_outputDir;
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
    if (!m_cacheValid) {
        outputDir(); // 触发缓存加载
    }
    return m_autoOpen;
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
    if (!m_cacheValid) {
        outputDir(); // 触发缓存加载
    }
    return m_filenameTemplate;
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

bool ConfigManager::keepTitleExtension() const
{
    if (!m_cacheValid) {
        outputDir(); // 触发缓存加载
    }
    return m_keepTitleExtension;
}

void ConfigManager::setKeepTitleExtension(bool keep)
{
    if (keep == keepTitleExtension()) {
        return;
    }
    QSettings settings = openSettings();
    settings.setValue(kKeyKeepTitleExtension, keep);
    syncConfig();
    emit keepTitleExtensionChanged();
}

void ConfigManager::syncConfig()
{
    QSettings settings = openSettings();
    settings.sync();
    invalidateCache();
}

void ConfigManager::invalidateCache()
{
    m_cacheValid = false;
}
