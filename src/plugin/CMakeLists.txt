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
dcc_install_plugin(NAME ${PLUGIN_NAME} TARGET ${PLUGIN_NAME})

# 翻译处理
dcc_handle_plugin_translation(NAME ${PLUGIN_NAME}
    SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}
)
