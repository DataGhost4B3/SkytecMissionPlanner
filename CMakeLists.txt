cmake_minimum_required(VERSION 3.28)
project(MP LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(GDAL REQUIRED)
find_package(PROJ REQUIRED)
find_package(CURL REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(Qt6 REQUIRED COMPONENTS Core Gui Quick Qml Concurrent Widgets)
qt_standard_project_setup()

#add_executable(MP main.cpp)
qt_add_executable(MP main.cpp)
set_target_properties(MP PROPERTIES OUTPUT_NAME "MissionPlanner")

#qt_add_qml_module(MP
#        URI MP
#        VERSION 1.0
#        RESOURCE_PREFIX "/qt/qml"
#        QML_FILES
#            "${CMAKE_CURRENT_SOURCE_DIR}/Main.qml"
#)

add_subdirectory(engine)
add_subdirectory(powertrain)
add_subdirectory(handle)
add_subdirectory(ui)

target_link_libraries(MP PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Quick
        Qt6::Qml
        Qt6::Concurrent
        Qt6::Widgets
        GDAL::GDAL
        MissionControl       # <-- Link handle library here too
        MissionControlplugin # <-- Link handle QML plugin here too
        MainUI
        MainUIplugin
)

#qt_add_qml_module(MP
#        URI ui
#        VERSION 1.0
#        RESOURCE_PREFIX "/qt/qml"
#        QML_FILES
#            ui/Main.qml
#            ui/MainForm.ui.qml
#)