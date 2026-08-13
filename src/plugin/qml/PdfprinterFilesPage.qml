import QtQuick 2.15
import QtQuick.Controls 2.0
import QtQuick.Layouts 1.15

import org.deepin.dcc 1.0

// PDF 文件列表页：显示输出目录中的 PDF 文件（图标 + 名称 + 大小 + 修改时间），
// 交互：点击整行或『打开』按钮打开文件，『删除』按钮（带确认）删除文件，顶部刷新 / 打开目录。
DccObject {
    id: root

    // 待删除的文件索引（点击删除按钮时暂存，确认后执行）
    property int pendingDeleteIndex: -1

    // 删除确认弹窗（纯 QML Popup 实现，Controls 2.0 均有，不依赖 Widgets）
    Popup {
        id: deleteDialog
        modal: true
        focus: true
        anchors.centerIn: parent
        width: 360
        padding: 20

        ColumnLayout {
            width: parent.width
            spacing: 16

            Text {
                Layout.fillWidth: true
                text: qsTr("确认删除")
                font.pixelSize: 16
                font.bold: true
                color: sysPal.windowText
            }

            Text {
                Layout.fillWidth: true
                text: qsTr("确定要删除这个 PDF 文件吗？此操作不可撤销。")
                wrapMode: Text.WordWrap
                color: sysPal.windowText
                opacity: 0.8
            }

            RowLayout {
                Layout.alignment: Qt.AlignRight
                spacing: 8

                Button {
                    text: qsTr("取消")
                    implicitWidth: 80
                    onClicked: {
                        root.pendingDeleteIndex = -1;
                        deleteDialog.close();
                    }
                }
                Button {
                    text: qsTr("删除")
                    implicitWidth: 80
                    highlighted: true
                    onClicked: {
                        if (root.pendingDeleteIndex >= 0) {
                            dccData.deletePdfFile(root.pendingDeleteIndex);
                        }
                        root.pendingDeleteIndex = -1;
                        deleteDialog.close();
                    }
                }
            }
        }
    }

    // 系统主题色（跟随亮/暗色，兼容 Qt5/Qt6 的纯 QtQuick 取色方式）
    SystemPalette {
        id: sysPal
        colorGroup: SystemPalette.Active
    }

    // ============ 文件列表组 ============
    DccObject {
        name: "fileGroup"
        parentName: root.name
        displayName: qsTr("已生成的 PDF 文件")
        weight: 10
        pageType: DccObject.Item
        page: DccGroupView {}

        // 空状态提示
        DccObject {
            name: "empty"
            parentName: root.name + "/fileGroup"
            displayName: qsTr("暂无 PDF 文件")
            weight: 10
            visible: dccData.pdfFileDetails.length === 0
            pageType: DccObject.Item
            page: Item {
                implicitHeight: 90
                ColumnLayout {
                    anchors.centerIn: parent
                    spacing: 8
                    Text {
                        Layout.alignment: Qt.AlignHCenter
                        text: "\U0001F5CE" // 🗎
                        font.pixelSize: 36
                        opacity: 0.35
                    }
                    Text {
                        Layout.alignment: Qt.AlignHCenter
                        text: qsTr("暂无 PDF 文件，打印任意文档后生成的 PDF 将显示在这里。")
                        opacity: 0.6
                    }
                }
            }
        }

        // 文件项列表（modelData 为 {name, size, sizeText, mtimeText, path}）
        DccRepeater {
            model: dccData.pdfFileDetails

            delegate: DccObject {
                name: "file" + index
                parentName: root.name + "/fileGroup"
                displayName: modelData.name
                weight: 20 + index
                backgroundType: DccObject.ClickStyle // 整行点击背景（自带 hover 高亮）
                pageType: DccObject.Item

                page: Item {
                    id: fileRow
                    implicitHeight: 52

                    // 悬停跟踪：只提供 hover 反馈与手型光标，不拦截鼠标事件，
                    // 整行点击仍由 DccObject.ClickStyle 的 onActive 处理。
                    MouseArea {
                        id: hoverArea
                        anchors.fill: parent
                        hoverEnabled: true
                        acceptedButtons: Qt.NoButton
                        cursorShape: Qt.PointingHandCursor
                    }

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 10
                        anchors.rightMargin: 8
                        spacing: 10

                        // 文件图标（简单图形：圆角 PDF 块，hover 时高亮）
                        Rectangle {
                            id: iconTile
                            implicitWidth: 36
                            implicitHeight: 36
                            radius: 8
                            border.width: 1
                            border.color: hoverArea.hovered ? sysPal.highlight : sysPal.mid
                            color: hoverArea.hovered ? Qt.rgba(sysPal.highlight.r, sysPal.highlight.g, sysPal.highlight.b, 0.15) : sysPal.alternateBase

                            Text {
                                anchors.centerIn: parent
                                text: "PDF"
                                font.pixelSize: 10
                                font.bold: true
                                color: hoverArea.hovered ? sysPal.highlight : sysPal.windowText
                            }
                        }

                        // 文件名（主）+ 大小 / 修改时间（次）
                        ColumnLayout {
                            Layout.fillWidth: true
                            Layout.alignment: Qt.AlignVCenter
                            spacing: 2

                            Text {
                                Layout.fillWidth: true
                                text: modelData.name
                                elide: Text.ElideRight
                                font.pixelSize: 14
                                color: sysPal.windowText
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 12

                                Text {
                                    text: modelData.sizeText
                                    font.pixelSize: 12
                                    color: sysPal.windowText
                                    opacity: 0.6
                                }
                                Text {
                                    text: modelData.mtimeText
                                    font.pixelSize: 12
                                    color: sysPal.windowText
                                    opacity: 0.6
                                }
                            }
                        }

                        // 打开按钮：Button 内部 MouseArea 消费鼠标按压事件，
                        // 不会冒泡到行级 onActive，避免重复触发打开。
                        Button {
                            Layout.alignment: Qt.AlignVCenter
                            implicitWidth: 64
                            implicitHeight: 32
                            text: qsTr("打开")
                            onClicked: dccData.openPdfFile(index)
                        }

                        // 删除按钮（点击后弹出确认对话框，防误删）
                        Button {
                            Layout.alignment: Qt.AlignVCenter
                            implicitWidth: 64
                            implicitHeight: 32
                            text: qsTr("删除")
                            onClicked: {
                                root.pendingDeleteIndex = index;
                                deleteDialog.open();
                            }
                        }
                    }

                    // 注：ToolTip 附加属性在普通 Item 根上无效（Unable to assign [undefined] to bool），
                    // 会导致 delegate 求值中断、按钮交互失效——已移除
                }

                // 点击整行打开文件
                onActive: dccData.openPdfFile(index)
            }
        }
    }

    // ============ 顶部操作 ============
    DccObject {
        name: "actions"
        parentName: root.name
        displayName: qsTr("操作")
        weight: 20
        pageType: DccObject.Item
        page: RowLayout {
            spacing: 10
            Button {
                text: qsTr("刷新")
                onClicked: dccData.refreshPdfList()
            }
            Button {
                text: qsTr("打开目录")
                onClicked: dccData.openOutputDir()
            }
        }
    }
}
