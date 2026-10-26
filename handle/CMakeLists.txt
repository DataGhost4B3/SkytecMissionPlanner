# handle/CMakeLists.txt

# 1. Change to qt_add_library
qt_add_library(MissionControl STATIC)

# 2. Add the qml module definition
qt_add_qml_module(MissionControl
        URI handle
        VERSION 1.0
        RESOURCE_PREFIX "/qt/qml"
        SOURCES
            missioncontrol.cpp
            missioncontrol.hpp
        RESOURCES
            kml_document.xml
            kml_placemark.xml
)

target_include_directories(MissionControl PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(MissionControl PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Qml
        Qt6::Quick
        Qt6::Concurrent
        Powertrain
)
