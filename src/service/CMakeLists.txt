cmake_minimum_required(VERSION 3.16)
project(pdfprinter-service LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui DBus)

add_library(pdfprinter-service STATIC
    printermanager.cpp
    configmanager.cpp
    outputdirwatcher.cpp
)

target_link_libraries(pdfprinter-service PUBLIC
    Qt6::Core
    Qt6::Gui
    Qt6::DBus
)

target_include_directories(pdfprinter-service PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)
