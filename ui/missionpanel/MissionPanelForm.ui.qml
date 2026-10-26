import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Frame {
    id: root

    // property alias mapCenterField: mapCenterField
    // property alias mapCenterButton: mapCenterButton

    // Expose interactive elements to the parent
    property alias lzNameField: lzName
    property alias latitudeField: latitudeField
    property alias longitudeField: longitudeField
    property alias generateButton: generateButton

    property alias elevationField: elevationField
    property alias dateField: dateField // ADD THIS
    property alias timeField: timeField // ADD THIS
    property alias jumpAltField: jumpAltField
    property alias canopyAltField: canopyAltField
    property alias weightField: weightField
    property alias airSpeedField: airSpeedField
    property alias airHeadingField: airHeadingField
    property alias canopyGroundSpeedField: canopyGroundSpeedField
    property alias glideRatioField: glideRatioField

    Layout.preferredWidth: 270
    Layout.fillHeight: true

    // Wrap the content in a ScrollView to enable vertical scrolling
    ScrollView {
        anchors.fill: parent
        contentWidth: width // Prevents horizontal scrolling
        clip: true // Hides content that scrolls outside the panel bounds

        ColumnLayout {
            // Bind width to the ScrollView, removing anchors.fill so it can grow vertically
            width: parent.width
            spacing: 0

            Label {
                text: "MISSION"
                font.bold: true
                font.pixelSize: 16
                Layout.bottomMargin: 10
            }

            ToolButton {
                id: lzHeader
                text: checked ? "▼ LZ Parameters" : "▶ LZ Parameters"
                font.bold: true
                Layout.fillWidth: true
                checkable: true
                checked: true
            }
            ColumnLayout {
                Layout.fillWidth: true
                visible: lzHeader.checked

                Layout.leftMargin: 12
                Layout.topMargin: 5
                Layout.bottomMargin: 15
                spacing: 8

                Label { text: "LZ Name" }
                TextField {
                    id: lzName
                    Layout.fillWidth: true
                    placeholderText: "Area 51"
                }
                Label { text: "Latitude (degrees)" }
                TextField {
                    id: latitudeField
                    Layout.fillWidth: true
                    placeholderText: "Latitude"
                    validator: RegularExpressionValidator {
                        regularExpression: /^-?(?:90(?:\.0*)?|[0-8]?\d(?:\.\d*)?)$/
                    }
                }
                Label { text: "Longitude (degrees)" }
                TextField {
                    id: longitudeField
                    Layout.fillWidth: true
                    placeholderText: "Longitude"
                    validator: RegularExpressionValidator {
                        regularExpression: /^-?(?:180(?:\.0*)?|1[0-7]\d(?:\.\d*)?|[0-9]?\d(?:\.\d*)?)$/
                    }
                }
                Label { text: "Elevation (meters)" }
                TextField {
                    id: elevationField
                    Layout.fillWidth: true
                    placeholderText: "200"
                }

                // ADD THESE TWO BLOCKS:
                Label { text: "Date (YYYY-MM-DD UTC)" }
                TextField {
                    id: dateField
                    Layout.fillWidth: true
                    placeholderText: "2026-10-25"
                }
                Label { text: "Time (HH:MM UTC)" }
                TextField {
                    id: timeField
                    Layout.fillWidth: true
                    placeholderText: "12:00"
                }
            }

            ToolButton {
                id: jumpHeader
                text: checked ? "▼ Jump Parameters" : "▶ Jump Parameters"
                font.bold: true
                Layout.fillWidth: true
                checkable: true
                checked: true
            }
            ColumnLayout {
                Layout.fillWidth: true
                visible: jumpHeader.checked

                Layout.leftMargin: 12
                Layout.topMargin: 5
                Layout.bottomMargin: 15
                spacing: 8

                Label { text: "Jump Altitude (ft)" }
                TextField{
                    id: jumpAltField
                    Layout.fillWidth: true
                    placeholderText: "7200"
                }

                Label { text: "Canopy Opening Altitude (ft)" }
                TextField{
                    id: canopyAltField
                    Layout.fillWidth: true
                    placeholderText: "5500"
                }

                Label { text: "Net Weight (kg)" }
                TextField{
                    id: weightField
                    Layout.fillWidth: true
                    placeholderText: "92"
                }
            }

            ToolButton {
                id: aircraftHeader
                text: checked ? "▼ Aircraft Parameters" : "▶ Aircraft Parameters"
                font.bold: true
                Layout.fillWidth: true
                checkable: true
                checked: true
            }
            ColumnLayout{
                Layout.fillWidth: true
                visible: aircraftHeader.checked

                Layout.leftMargin: 12
                Layout.topMargin: 5
                Layout.bottomMargin: 15
                spacing: 8

                Label { text: "Speed (kts)" }
                TextField {
                    id: airSpeedField
                    Layout.fillWidth: true
                }
                Label { text: "Heading (deg)" }
                TextField {
                    id: airHeadingField
                    Layout.fillWidth: true
                }
            }

            ToolButton {
                id: parachuteHeader
                text: checked ? "▼ Parachute Parameters" : "▶ Parachute Parameters"
                font.bold: true
                Layout.fillWidth: true
                checkable: true
                checked: true
            }
            ColumnLayout {
                Layout.fillWidth: true
                visible: parachuteHeader.checked

                Layout.leftMargin: 12
                Layout.topMargin: 5
                Layout.bottomMargin: 15
                spacing: 8

                Label { text: "Ground Speed (kts)" }
                TextField {
                    id: canopyGroundSpeedField
                    Layout.fillWidth: true
                }
                Label { text: "Glide Ratio" }
                TextField {
                    id: glideRatioField
                    Layout.fillWidth: true
                }
            }

            Button {
                id: generateButton
                text: "Run"
                Layout.fillWidth: true
                Layout.topMargin: 10 // Added a slight margin to separate it from the expanding groups
            }

            Item { Layout.fillHeight: true }
        }
    }
}