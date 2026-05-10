cmake_minimum_required(VERSION 3.16)
project(tts LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Network Multimedia)

if(APPLE)
    set(MACOSX_BUNDLE TRUE)
endif()

add_executable(tts
    main.cpp
    mainwindow.h mainwindow.cpp
    config.h config.cpp
    ttsengine.h ttsengine.cpp
    voicesettingsdialog.h voicesettingsdialog.cpp
)

target_link_libraries(tts PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Network
    Qt6::Multimedia
)
