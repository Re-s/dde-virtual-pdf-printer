import org.deepin.dcc 1.0

// 根模块元数据：此文件不能使用 dccData（C++ 插件此时尚未加载）
DccObject {
    name: "pdfprinter"          // 与 CMake PLUGIN_NAME 一致
    parentName: "root"
    displayName: qsTr("PDF 打印机")
    icon: "dcc_pdfprinter"
    weight: 100
}
