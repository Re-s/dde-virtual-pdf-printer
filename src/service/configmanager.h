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
    QString filenameTemplate() const;  // 输出文件名模板（默认 {title}-{jobid}-{date}-{time}）
    void setFilenameTemplate(const QString &tpl);
    bool keepTitleExtension() const;   // 是否保留原文档后缀（默认 false）
    void setKeepTitleExtension(bool keep);

Q_SIGNALS:
    void outputDirChanged();
    void autoOpenChanged();
    void filenameTemplateChanged();
    void keepTitleExtensionChanged();

private:
    void syncConfig();                 // 写回配置存储
    void invalidateCache();            // 使缓存失效

    mutable bool m_cacheValid{false};
    mutable QString m_outputDir;
    mutable bool m_autoOpen{false};
    mutable QString m_filenameTemplate;
    mutable bool m_keepTitleExtension{false};
};
