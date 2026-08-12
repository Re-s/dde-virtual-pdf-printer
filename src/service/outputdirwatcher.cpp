#include "outputdirwatcher.h"

OutputDirWatcher::OutputDirWatcher(QObject *parent)
    : QObject(parent)
{
    // Forward any directory change to the public filesChanged signal.
    connect(&m_watcher, &QFileSystemWatcher::directoryChanged, this, [this] {
        emit filesChanged();
    });
}

void OutputDirWatcher::watch(const QString &dir)
{
    if (dir.isEmpty() || m_watcher.directories().contains(dir)) {
        return;
    }
    // Only watch one directory at a time; drop previous watches first.
    const QStringList watched = m_watcher.directories();
    if (!watched.isEmpty()) {
        m_watcher.removePaths(watched);
    }
    m_watcher.addPath(dir);
}
