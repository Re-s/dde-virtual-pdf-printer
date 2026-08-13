#pragma once
#include <QObject>
#include <QStringList>

class ConfigManager;

class PrinterManager : public QObject
{
    Q_OBJECT
public:
    explicit PrinterManager(ConfigManager *config, QObject *parent = nullptr);

    // 状态查询
    bool printerExists() const;        // lpstat 检测 DDE-PDF 打印机
    bool isEnabled() const;            // 打印机是否启用
    QString printerName() const;       // 固定返回 "DDE-PDF"
    QString outputDir() const;         // 读取配置的输出目录（默认 ~/PDF）

    // 操作（返回是否成功）
    bool createPrinter();              // lpadmin -p DDE-PDF -E -v ddepdf:/ -P Generic-PDF PPD
    bool removePrinter();              // lpadmin -x DDE-PDF
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
    ConfigManager *m_config = nullptr;
};
