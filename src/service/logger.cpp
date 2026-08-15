// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later
#include "logger.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QTextStream>

void pdfprinterWriteLog(const QString &msg)
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    QDir().mkpath(dir);
    const QString path = dir + QStringLiteral("/pdfprinter.log");
    // 日志大小上限 512KB：超限清空重建（防无限增长磁盘 DoS）
    if (QFileInfo::exists(path) && QFileInfo(path).size() > 512 * 1024) {
        QFile::remove(path);
    }
    QFile f(path);
    if (f.open(QIODevice::Append | QIODevice::Text)) {
        // 仅当前用户可读写（默认 umask 下的追加模式可能为 644）
        f.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);
        QTextStream ts(&f);
        ts << QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd hh:mm:ss.zzz"))
           << QLatin1Char(' ') << msg << QLatin1Char('\n');
    }
}
