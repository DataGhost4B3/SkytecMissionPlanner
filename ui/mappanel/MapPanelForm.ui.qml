import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Frame {
    id: root
    // Layout.preferredWidth: 270
    // Layout.fillHeight: true

    // Removes the white background and black frame border
    background: Item {}

    property alias mapCheck: mapCheck
    // property alias aoiCheck: aoiCheck
    property alias elevationCheck: elevationCheck
    property alias downloadMap: downloadMap
    property alias sizeEstimateLabel: sizeEstimateLabel

    // ADD THESE:
    property alias centerLatField: centerLatField
    property alias centerLonField: centerLonField
    property alias viewWidthField: viewWidthField
    property alias applyViewButton: applyViewButton

    ScrollView {
        anchors.fill: parent
        contentWidth: width // Prevents horizontal scrolling
        clip: true // Hides content that scrolls outside the panel bounds

        ColumnLayout {
            // anchors.fill: parent
            width: parent.width
            spacing: 12

            Label {
                text: "MAP"
                font.bold: true
                font.pixelSize: 16
            }

            CheckBox {
                id: mapCheck
                text: "Map"
                checked: true
                visible: false
            }

            // CheckBox {
            //     id: aoiCheck
            //     text: "AOI boundary"
            //     checked: true
            // }

            CheckBox {
                id: elevationCheck
                text: "Elevation contours"
                checked: false
                visible: false
            }

            Label {
                id: sizeEstimateLabel
                text: "Est. Cache Size: 0 MB"
                color: "#666666"
                font.pixelSize: 11
            }

            // ADD THIS BLOCK:
            Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: "#dddddd" }

            Label { text: "Canvas Center (Lat / Lon)" }
            RowLayout {
                Layout.fillWidth: true
                TextField { id: centerLatField; Layout.fillWidth: true; placeholderText: "Latitude" }
                TextField { id: centerLonField; Layout.fillWidth: true; placeholderText: "Longitude" }
            }

            Label { text: "Camera View Width (km)" }
            RowLayout {
                Layout.fillWidth: true
                TextField { id: viewWidthField; Layout.fillWidth: true; placeholderText: "Width in km" }
                Button { id: applyViewButton; text: "Go" }
            }

            Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: "#dddddd" }
            // END BLOCK

            Button {
                id: downloadMap
                text: "Download map with checked features"
            }

            Item {
                Layout.fillHeight: true
            }
        }
    }
}