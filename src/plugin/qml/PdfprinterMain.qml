import org.deepin.dcc 1.0

// 主页面：组合 4 个子页面（状态 / PDF 文件列表 / 设置 / 帮助）
DccObject {
    PdfprinterStatusPage {
        name: "status"
        parentName: "pdfprinter"
        displayName: qsTr("打印机状态")
        icon: "dcc_pdfprinter"
        weight: 10
    }

    PdfprinterFilesPage {
        name: "files"
        parentName: "pdfprinter"
        displayName: qsTr("PDF 文件")
        icon: "dcc_pdfprinter"
        weight: 20
    }

    PdfprinterSettingsPage {
        name: "settings"
        parentName: "pdfprinter"
        displayName: qsTr("设置")
        icon: "dcc_pdfprinter"
        weight: 30
    }

    PdfprinterHelpPage {
        name: "help"
        parentName: "pdfprinter"
        displayName: qsTr("帮助")
        icon: "dcc_pdfprinter"
        weight: 40
    }
}
