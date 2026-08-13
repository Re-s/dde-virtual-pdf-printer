import QtQuick 2.15
import QtQuick.Controls 2.0
import QtQuick.Layouts 1.15

import org.deepin.dcc 1.0

// 设置页：输出目录 + 打印后自动打开
DccObject {
    id: root

    // 监听异步目录选择结果（C++ 通过 D-Bus 调用 deepin 原生目录对话框）
    Connections {
        target: dccData
        function onOutputDirPicked(dir) {
            if (dir !== "") {
                // 注意：setOutputDir 是 Q_PROPERTY WRITE，QML 中必须用属性赋值
                dccData.outputDir = dir;
            }
        }
    }

    DccObject {
        name: "settingsGroup"
        parentName: root.name
        displayName: qsTr("设置")
        weight: 10
        pageType: DccObject.Item
        page: DccGroupView {}

        DccObject {
            name: "outputDir"
            parentName: root.name + "/settingsGroup"
            displayName: qsTr("输出目录")
            description: qsTr("生成的 PDF 文件保存位置，默认 ~/PDF，可自定义")
            weight: 10
            pageType: DccObject.Editor
            page: RowLayout {
                spacing: 8
                // 路径框尽量占满（完整显示路径），按钮紧凑
                TextField {
                    id: dirField
                    Layout.fillWidth: true
                    implicitWidth: 320
                    readOnly: true
                    text: dccData.outputDir
                }
                Button {
                    text: qsTr("选择目录…")
                    implicitWidth: 84
                    onClicked: {
                        // C++ 侧通过 D-Bus 异步弹出 deepin 原生目录选择对话框，
                        // 结果经 onOutputDirPicked 信号回写（不阻塞主线程）
                        dccData.openOutputDirPicker();
                    }
                }
                Button {
                    text: qsTr("恢复默认")
                    implicitWidth: 74
                    onClicked: dccData.outputDir = dccData.defaultOutputDir()
                }
            }
        }

        DccObject {
            name: "autoOpen"
            parentName: root.name + "/settingsGroup"
            displayName: qsTr("打印后自动打开 PDF")
            description: qsTr("开启后，打印完成的 PDF 将自动用默认应用打开")
            weight: 20
            pageType: DccObject.Editor
            page: Switch {
                checked: dccData.autoOpen
                onToggled: dccData.autoOpen = checked
            }
        }

        DccObject {
            name: "filenameTemplate"
            parentName: root.name + "/settingsGroup"
            displayName: qsTr("默认文件名")
            description: qsTr("PDF 文件名模板，支持占位符：{title} 原文档名 / {jobid} 作业号 / {date} 日期 / {time} 时间")
            weight: 30
            pageType: DccObject.Editor
            page: RowLayout {
                spacing: 8
                TextField {
                    id: tplField
                    Layout.fillWidth: true
                    implicitWidth: 300
                    placeholderText: "{title}-{jobid}-{date}-{time}"
                    text: dccData.filenameTemplate
                    // 编辑完成（回车/失焦）时保存；属性赋值而非 set 方法（WRITE 属性）
                    onEditingFinished: {
                        if (text !== dccData.filenameTemplate) {
                            dccData.filenameTemplate = text;
                        }
                    }
                }
                Button {
                    text: qsTr("恢复默认")
                    implicitWidth: 74
                    onClicked: dccData.filenameTemplate = "{title}-{jobid}-{date}-{time}"
                }
            }
        }

        DccObject {
            name: "keepTitleExtension"
            parentName: root.name + "/settingsGroup"
            displayName: qsTr("保留原文件后缀")
            description: qsTr("开启后文件名保留原始文档后缀（如 采购单.txt），默认不保留（如 采购单）")
            weight: 40
            pageType: DccObject.Editor
            page: Switch {
                checked: dccData.keepTitleExtension
                onToggled: dccData.keepTitleExtension = checked
            }
        }
    }
}
