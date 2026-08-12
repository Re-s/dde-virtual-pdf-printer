cmake_minimum_required(VERSION 3.23)
set(CMAKE_AUTOMOC ON)
find_package(Qt6 COMPONENTS Core Widgets REQUIRED)
find_package(DdeControlCenter REQUIRED)
add_library(pdfprinter-operation MODULE pdfprintermodule.cpp)
# Same hint dcc_install_plugin applies: DCC_FACTORY_CLASS expands to a Q_OBJECT class
set_property(TARGET pdfprinter-operation APPEND PROPERTY AUTOMOC_MACRO_NAMES "DCC_FACTORY_CLASS")
target_link_libraries(pdfprinter-operation PRIVATE Dde::Control-Center Qt6::Core Qt6::Widgets)
target_include_directories(pdfprinter-operation PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../service)
