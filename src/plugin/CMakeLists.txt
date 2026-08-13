cmake_minimum_required(VERSION 3.23)
project(pdfprinter-plugin LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
# 静态库（service）需 PIC 才能链进插件 .so：AArch64 严格要求 -fPIC（x86_64 侥幸通过，
# arm64 链接报 dangerous relocation）
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(PLUGIN_NAME "pdfprinter")

find_package(Qt6 COMPONENTS Core DBus LinguistTools REQUIRED)
find_package(DdeControlCenter REQUIRED)
find_package(Dtk6 COMPONENTS Core Gui REQUIRED)

# 服务层静态库
add_subdirectory(../service service)

# 插件模块
file(GLOB_RECURSE PLUGIN_SRCS "operation/*.cpp" "operation/*.h")
add_library(${PLUGIN_NAME} MODULE ${PLUGIN_SRCS})
set_property(TARGET ${PLUGIN_NAME} APPEND PROPERTY AUTOMOC_MACRO_NAMES "DCC_FACTORY_CLASS")

target_link_libraries(${PLUGIN_NAME} PRIVATE
    Dde::Control-Center
    pdfprinter-service
    Qt6::Core
    Qt6::DBus
    Dtk6::Core
    Dtk6::Gui
)

target_include_directories(${PLUGIN_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../service
)

# 安装插件（自动收集 qml/ 下所有资源）
dcc_install_plugin(NAME ${PLUGIN_NAME} TARGET ${PLUGIN_NAME})

# 安装翻译（qm 文件到 DCC 翻译目录）
install(FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/translations/pdfprinter_zh_CN.qm
    DESTINATION ${CMAKE_INSTALL_DATADIR}/dde-control-center/translations/v1.1/
)

