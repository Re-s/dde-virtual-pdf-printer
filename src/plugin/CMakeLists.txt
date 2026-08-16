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

# 插件模块（C++ DccFactory 数据对象）
file(GLOB_RECURSE PLUGIN_SRCS "operation/*.cpp" "operation/*.h")
add_library(${PLUGIN_NAME} MODULE ${PLUGIN_SRCS})

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

# 使用官方宏：自动处理 QML 模块（qt_add_qml_module）、资源编译、安装路径、qmldir 生成
# 注意：旧版 DCC（deepin beige）的 dcc_install_plugin 不支持 qt_add_qml_module，
# 需要检测并回退到手动构建
include(CheckSymbolExists)
set(CMAKE_REQUIRED_INCLUDES ${CMAKE_CURRENT_SOURCE_DIR})
check_symbol_exists(dcc_build_plugin "DdeControlCenterPluginMacros.cmake" HAS_DCC_BUILD_PLUGIN)

# 检测 DCC 宏是否支持 qt_add_qml_module（通过检查宏文件内容）
file(READ "${DdeControlCenter_DIR}/DdeControlCenterPluginMacros.cmake" DCC_MACRO_CONTENT)
string(FIND "${DCC_MACRO_CONTENT}" "qt_add_qml_module" DCC_HAS_QML_MODULE)

if(DCC_HAS_QML_MODULE GREATER -1)
    # 新版 DCC：直接使用官方宏
    dcc_install_plugin(NAME ${PLUGIN_NAME} TARGET ${PLUGIN_NAME})
else()
    # 旧版 DCC（beige）：手动构建 QML 模块
    message(STATUS "DCC macro does not support qt_add_qml_module, using manual QML build")

    # 手动构建 QML 模块
    file(GLOB_RECURSE QML_FILES "${CMAKE_CURRENT_SOURCE_DIR}/qml/*.qml")
    set(QRC_CONTENT "<RCC>\n  <qresource prefix=\"/qt/qml/${PLUGIN_NAME}\">\n")
    foreach(QML_FILE ${QML_FILES})
        get_filename_component(QML_NAME ${QML_FILE} NAME)
        string(APPEND QRC_CONTENT "    <file alias=\"${QML_NAME}\">${QML_FILE}</file>\n")
    endforeach()
    string(APPEND QRC_CONTENT "  </qresource>\n</RCC>\n")
    set(QRC_FILE "${CMAKE_CURRENT_BINARY_DIR}/${PLUGIN_NAME}_qml.qrc")
    file(WRITE "${QRC_FILE}" "${QRC_CONTENT}")

    # 预生成 QML 资源 C++ 文件
    set(QRC_CPP "${CMAKE_CURRENT_BINARY_DIR}/${PLUGIN_NAME}_qml_res.cpp")
    find_program(RCC_EXECUTABLE NAMES rcc HINTS /usr/lib/qt6/libexec /usr/lib/qt6/bin /usr/bin)
    execute_process(
        COMMAND ${RCC_EXECUTABLE} --name ${PLUGIN_NAME}_qml --output "${QRC_CPP}" "${QRC_FILE}"
        RESULT_VARIABLE RCC_RESULT
    )
    if(NOT RCC_RESULT EQUAL 0)
        message(FATAL_ERROR "rcc failed for ${PLUGIN_NAME} QML resources")
    endif()

    # 构建 QML 模块
    add_library(${PLUGIN_NAME}_qml MODULE qml/qmldir.cpp "${QRC_CPP}")
    target_link_libraries(${PLUGIN_NAME}_qml PRIVATE Qt6::Core Qt6::Qml)

    # 安装路径（跟随系统 DCC 配置）
    set(PLUGIN_DIR ${DDE_CONTROL_CENTER_PLUGIN_DIR})
    set(PLUGIN_INSTALL_DIR ${DDE_CONTROL_CENTER_PLUGIN_INSTALL_DIR})

    # 安装 C++ 插件
    install(TARGETS ${PLUGIN_NAME}
        LIBRARY DESTINATION ${PLUGIN_INSTALL_DIR}/${PLUGIN_NAME}/
    )

    # 安装 QML 模块
    install(TARGETS ${PLUGIN_NAME}_qml
        LIBRARY DESTINATION ${PLUGIN_INSTALL_DIR}/${PLUGIN_NAME}/
    )

    # 安装 qmldir
    install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/qml/qmldir
        DESTINATION ${PLUGIN_INSTALL_DIR}/${PLUGIN_NAME}/
    )

    # 安装 QML 源文件（控制中心需要从文件系统加载）
    install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/qml/
        DESTINATION ${PLUGIN_INSTALL_DIR}/${PLUGIN_NAME}/
        FILES_MATCHING PATTERN "*.qml"
    )
endif()

# 翻译处理
dcc_handle_plugin_translation(NAME ${PLUGIN_NAME}
    SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}
)
