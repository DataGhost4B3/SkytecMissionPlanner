
qt_add_library(MainUI STATIC)

qt_add_qml_module(MainUI
        URI ui
        VERSION 1.0
        RESOURCE_PREFIX "/qt/qml"
        QML_FILES
            Main.qml
            MainForm.ui.qml

            missionpanel/MissionPanel.qml
            missionpanel/MissionPanelForm.ui.qml

            mapcanvas/MapCanvas.qml
            mapcanvas/MapCanvasForm.ui.qml

            mappanel/MapPanel.qml
            mappanel/MapPanelForm.ui.qml

            windpanel/WindPanel.qml
            windpanel/WindPanelForm.ui.qml
        SOURCES
            mapcanvas/mapcanvas.hpp
            mapcanvas/mapcanvas.cpp
        RESOURCES
            mapcanvas/osm_config.xml
        DEPENDENCIES
            handle
)

target_link_libraries(MainUI PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Qml
        Qt6::Quick
        Qt6::Concurrent
        GDAL::GDAL
        PROJ::proj
        MissionControl        # <-- Link the C++ symbols
        MissionControlplugin  # <-- Link the QML registration backing plugin
)

target_include_directories(MainUI PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/mapcanvas
)