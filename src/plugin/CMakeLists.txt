cmake_minimum_required(VERSION 3.23)
project(pdfprinter-plugin LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
# 静态库（service）需 PIC 才能链进插件 .so：AArch64 严格要求 -fPIC（x86_64 侥幸通过，
# arm64 链接报 dangerous relocation）
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# 二进制加固（Debian/deepin hardening 规范，2026-08 安全审查发现缺 Stack Canary/FORTIFY/Full RELRO）：
# - -O2：FORTIFY 需要优化级（默认构建无 -O 时 _FORTIFY_SOURCE 无效）
# - -fstack-protector-strong：栈保护（Stack Canary）
# - -D_FORTIFY_SOURCE=2：缓冲区函数加固（_chk 包装）
# - -Wformat -Werror=format-security：格式串安全
# - -ffile-prefix-map：源码绝对路径映射为相对路径（防止本机路径泄漏进二进制，脱敏）
# - -z relro -z now：Full RELRO（GOT 只读）
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    get_filename_component(PROJ_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)
    add_compile_options(-O2 -fstack-protector-strong -D_FORTIFY_SOURCE=2
                        -Wformat -Werror=format-security
                        "-ffile-prefix-map=${PROJ_ROOT}=.")
    add_link_options(-Wl,-z,relro -Wl,-z,now)
endif()

set(PLUGIN_NAME "pdfprinter")

find_package(Qt6 COMPONENTS Core DBus Qml LinguistTools REQUIRED)
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

# QML 插件模块（编译 QML 为 Qt 资源嵌入 .so，控制中心通过 qrc:// 加载）
file(GLOB QML_FILES "${CMAKE_CURRENT_SOURCE_DIR}/qml/*.qml")
set(QRC_CONTENT "<RCC>\n  <qresource prefix=\"/qt/qml/${PLUGIN_NAME}\">\n")
foreach(QML_FILE ${QML_FILES})
    get_filename_component(QML_NAME ${QML_FILE} NAME)
    string(APPEND QRC_CONTENT "    <file alias=\"${QML_NAME}\">${QML_FILE}</file>\n")
endforeach()
string(APPEND QRC_CONTENT "  </qresource>\n</RCC>\n")
set(QRC_FILE "${CMAKE_CURRENT_BINARY_DIR}/${PLUGIN_NAME}_qml.qrc")
file(WRITE "${QRC_FILE}" "${QRC_CONTENT}")
# 预生成 QML 资源 C++ 文件（configure 阶段执行 rcc，避免 AUTOMOC 时文件不存在）
set(QRC_CPP "${CMAKE_CURRENT_BINARY_DIR}/${PLUGIN_NAME}_qml_res.cpp")
find_program(RCC_EXECUTABLE NAMES rcc HINTS /usr/lib/qt6/libexec /usr/lib/qt6/bin /usr/bin)
execute_process(
    COMMAND ${RCC_EXECUTABLE} --name ${PLUGIN_NAME}_qml --output "${QRC_CPP}" "${QRC_FILE}"
    RESULT_VARIABLE RCC_RESULT
)
if(NOT RCC_RESULT EQUAL 0)
    message(FATAL_ERROR "rcc failed for ${PLUGIN_NAME} QML resources: ${RCC_EXECUTABLE}")
endif()
add_library(${PLUGIN_NAME}_qml MODULE qml/qmldir.cpp "${QRC_CPP}")
target_link_libraries(${PLUGIN_NAME}_qml PRIVATE
    Qt6::Core
    Qt6::Qml
)

# 安装插件到 DCC v1.1 路径（dcc_install_plugin 宏可能生成 v1.0 路径，
# DCC 2:1.1+ 只扫描 plugins_v1.1/，v1.0 已弃用）
install(TARGETS ${PLUGIN_NAME}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}/dde-control-center/plugins_v1.1/${PLUGIN_NAME}/
    RENAME lib${PLUGIN_NAME}plugin.so
)
# 安装 QML 插件到 DCC v1.1 路径
install(TARGETS ${PLUGIN_NAME}_qml
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}/dde-control-center/plugins_v1.1/${PLUGIN_NAME}/
    RENAME lib${PLUGIN_NAME}_qml.so
)
# QML 模块定义文件（qmldir 与 .so 同目录，QML 内容已编译为资源嵌入 _qml.so）
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/qml/qmldir
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/dde-control-center/plugins_v1.1/${PLUGIN_NAME}/
)

# 安装翻译（qm 文件到 DCC 翻译目录）
install(FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/translations/pdfprinter_zh_CN.qm
    DESTINATION ${CMAKE_INSTALL_DATADIR}/dde-control-center/translations/v1.1/
)

