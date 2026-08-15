// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QString>

// Shared log writer used by both the plugin and the service layer.
// 控制中心 stdout/stderr 被 deepin 会话重定向到 /dev/null，
// 且 QT_LOGGING_RULES 屏蔽 qDebug/qInfo——必须直接写文件才能看到。
void pdfprinterWriteLog(const QString &msg);
