import QtQuick 2.15
import QtQuick.Controls 2.0
import QtQuick.Layouts 1.15

import org.deepin.dcc 1.0

// 帮助页：使用引导（三步说明 + 常见问题）
// 注意：每个子项必须有 page 组件（DccObject.Item 无 page 时显示空白）
DccObject {
    id: root

    // 系统主题色
    SystemPalette {
        id: sysPal
        colorGroup: SystemPalette.Active
    }

    DccObject {
        name: "helpGroup"
        parentName: root.name
        displayName: qsTr("使用说明")
        weight: 10
        pageType: DccObject.Item
        page: DccGroupView {}

        // 第一步
        DccObject {
            name: "step1"
            parentName: root.name + "/helpGroup"
            displayName: qsTr("① 打印")
            weight: 10
            pageType: DccObject.Editor
            page: Item {
                // 容器限制宽度，Text 在可用空间内换行（防止窄窗口盖住左侧标题）
                Layout.fillWidth: true
                Layout.maximumWidth: 480
                implicitWidth: 320
                implicitHeight: 44
                Text {
                    anchors.fill: parent
                    text: qsTr("在任意应用中打开打印对话框（Ctrl+P），打印机选择 Deepin-PDF")
                    wrapMode: Text.WordWrap
                    elide: Text.ElideRight
                    verticalAlignment: Text.AlignVCenter
                    color: sysPal.windowText
                }
            }
        }

        // 第二步
        DccObject {
            name: "step2"
            parentName: root.name + "/helpGroup"
            displayName: qsTr("② 输出")
            weight: 20
            pageType: DccObject.Editor
            page: Item {
                Layout.fillWidth: true
                Layout.maximumWidth: 480
                implicitWidth: 320
                implicitHeight: 44
                Text {
                    anchors.fill: parent
                    text: qsTr("点击打印，PDF 文件将保存到「输出目录」（默认 ~/PDF，可在设置页修改）")
                    wrapMode: Text.WordWrap
                    elide: Text.ElideRight
                    verticalAlignment: Text.AlignVCenter
                    color: sysPal.windowText
                }
            }
        }

        // 第三步
        DccObject {
            name: "step3"
            parentName: root.name + "/helpGroup"
            displayName: qsTr("③ 管理")
            weight: 30
            pageType: DccObject.Editor
            page: Item {
                Layout.fillWidth: true
                Layout.maximumWidth: 480
                implicitWidth: 320
                implicitHeight: 44
                Text {
                    anchors.fill: parent
                    text: qsTr("在「PDF 文件」页查看、打开、删除生成的 PDF；开启「打印后自动打开」可即时查看")
                    wrapMode: Text.WordWrap
                    elide: Text.ElideRight
                    verticalAlignment: Text.AlignVCenter
                    color: sysPal.windowText
                }
            }
        }
    }

    DccObject {
        name: "faqGroup"
        parentName: root.name
        displayName: qsTr("常见问题")
        weight: 20
        pageType: DccObject.Item
        page: DccGroupView {}

        DccObject {
            name: "faq1"
            parentName: root.name + "/faqGroup"
            displayName: qsTr("打印后没有生成 PDF？")
            weight: 10
            pageType: DccObject.Editor
            page: Item {
                Layout.fillWidth: true
                Layout.maximumWidth: 480
                implicitWidth: 320
                implicitHeight: 44
                Text {
                    anchors.fill: parent
                    text: qsTr("请先在「打印机状态」页确认 Deepin-PDF 打印机已安装（显示「已安装」）")
                    wrapMode: Text.WordWrap
                    elide: Text.ElideRight
                    verticalAlignment: Text.AlignVCenter
                    color: sysPal.windowText
                }
            }
        }

        DccObject {
            name: "faq2"
            parentName: root.name + "/faqGroup"
            displayName: qsTr("如何修改保存位置？")
            weight: 20
            pageType: DccObject.Editor
            page: Item {
                Layout.fillWidth: true
                Layout.maximumWidth: 480
                implicitWidth: 320
                implicitHeight: 44
                Text {
                    anchors.fill: parent
                    text: qsTr("在「设置」页点击「选择目录…」，选择任意文件夹作为输出目录")
                    wrapMode: Text.WordWrap
                    elide: Text.ElideRight
                    verticalAlignment: Text.AlignVCenter
                    color: sysPal.windowText
                }
            }
        }
    }
}
