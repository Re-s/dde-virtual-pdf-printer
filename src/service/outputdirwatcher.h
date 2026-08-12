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
