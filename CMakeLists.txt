cmake_minimum_required(VERSION 3.16)
project(ProteusPro VERSION 0.2 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets)

add_executable(ProteusPro
    main.cpp
    mainwindow.cpp
    mainwindow.h
    projectdocument.cpp
    projectdocument.h
    recentprojectsmanager.cpp
    recentprojectsmanager.h
    startmenudialog.cpp
    startmenudialog.h
)

target_link_libraries(ProteusPro PRIVATE Qt${QT_VERSION_MAJOR}::Widgets)

if(WIN32)
    set_target_properties(ProteusPro PROPERTIES WIN32_EXECUTABLE TRUE)
endif()
