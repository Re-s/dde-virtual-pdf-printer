#pragma once
#include <QObject>
#include <QStringList>
#include <QVariantList>

class PdfPrinterModule : public QObject
{
    Q_OBJECT
    // QML 可读属性
    Q_PROPERTY(bool printerExists READ printerExists NOTIFY printerStateChanged FINAL)
    Q_PROPERTY(QString printerName READ printerName CONSTANT FINAL)
    Q_PROPERTY(QString outputDir READ outputDir WRITE setOutputDir NOTIFY outputDirChanged FINAL)
    Q_PROPERTY(bool autoOpen READ autoOpen WRITE setAutoOpen NOTIFY autoOpenChanged FINAL)
    Q_PROPERTY(QString filenameTemplate READ filenameTemplate WRITE setFilenameTemplate NOTIFY filenameTemplateChanged FINAL)
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
    QString filenameTemplate() const;
    void setFilenameTemplate(const QString &tpl);
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
    Q_INVOKABLE void openOutputDirPicker();  // 异步弹出目录选择对话框，结果通过 outputDirPicked 信号返回

Q_SIGNALS:
    void printerStateChanged();
    void outputDirChanged();
    void autoOpenChanged();
    void filenameTemplateChanged();
    void pdfFilesChanged();
    void busyChanged();
    void lastErrorChanged();
    void pdfAutoOpened(const QString &filePath);  // autoOpen 生效时发出
    void outputDirPicked(const QString &dir);     // 目录选择完成（取消时为空串）

private Q_SLOTS:
    // D-Bus 文件对话框信号（异步目录选择）
    void onDialogAccepted();                      // accepted 信号（用户确认）
    void onDialogRejected();                      // rejected 信号（用户取消）

private:
    void cleanupDialog();  // 断开信号并销毁 D-Bus 对话框
    class Impl;  // PIMPL：组合 PrinterManager/ConfigManager/OutputDirWatcher
    Impl *d;
};
