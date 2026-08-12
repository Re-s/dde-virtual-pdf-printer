import QtQuick 2.15
import QtQuick.Controls 2.0
import QtQuick.Layouts 1.15

import org.deepin.dcc 1.0

// 打印机状态页：状态展示 + 安装/移除操作
DccObject {
    id: root

    DccObject {
        name: "statusGroup"
        parentName: root.name
        displayName: qsTr("打印机状态")
        weight: 10
        pageType: DccObject.Item
        page: DccGroupView {}

        DccObject {
            name: "state"
            parentName: root.name + "/statusGroup"
            displayName: qsTr("状态")
            weight: 10
            pageType: DccObject.Editor
            page: Text {
                text: dccData.printerExists ? qsTr("已安装") : qsTr("未安装")
            }
        }

        DccObject {
            name: "printerName"
            parentName: root.name + "/statusGroup"
            displayName: qsTr("打印机名称")
            weight: 20
            pageType: DccObject.Editor
            page: Text {
                text: dccData.printerName
            }
        }

        DccObject {
            name: "outputDir"
            parentName: root.name + "/statusGroup"
            displayName: qsTr("输出目录")
            weight: 30
            pageType: DccObject.Editor
            page: Text {
                text: dccData.outputDir
            }
        }
    }

    DccObject {
        name: "actionGroup"
        parentName: root.name
        displayName: qsTr("操作")
        weight: 20
        pageType: DccObject.Item
        page: DccGroupView {}

        DccObject {
            name: "install"
            parentName: root.name + "/actionGroup"
            displayName: qsTr("安装打印机")
            weight: 10
            visible: !dccData.printerExists
            pageType: DccObject.Editor
            page: Button {
                text: qsTr("安装打印机")
                onClicked: dccData.createPrinter()
            }
        }

        DccObject {
            name: "remove"
            parentName: root.name + "/actionGroup"
            displayName: qsTr("移除打印机")
            weight: 20
            visible: dccData.printerExists
            pageType: DccObject.Editor
            page: Button {
                text: qsTr("移除打印机")
                onClicked: dccData.removePrinter()
            }
        }
    }
}
