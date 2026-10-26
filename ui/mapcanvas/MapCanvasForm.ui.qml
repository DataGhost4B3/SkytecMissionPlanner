import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root

    // Expose dynamic labels
    // property alias aoiBoxLabel: aoiBoxLabel
    // property alias centerLabel: centerLabel
    property alias zoomInButton: zoomInButton
    property alias zoomOutButton: zoomOutButton
    property alias selectButton: selectButton
    property alias drawButton: drawButton

    property alias tPointButton: tPointButton

    Layout.fillWidth: true
    Layout.fillHeight: true
    // color: "#e8edf0"
    color: "transparent"
    clip: true

    // Column {
    //     anchors.centerIn: parent
    //     spacing: 12
    //
    //     // Label {
    //     //     anchors.horizontalCenter: parent.horizontalCenter
    //     //     text: "MAP VIEWPORT"
    //     //     font.pixelSize: 22
    //     //     font.bold: true
    //     //     color: "#455a64"
    //     // }
    //     //
    //     // Label {
    //     //     anchors.horizontalCenter: parent.horizontalCenter
    //     //     text: "Map rendering will be implemented here"
    //     //     color: "#607d8b"
    //     // }
    //
    //     // Rectangle {
    //     //     width: 220; height: 220
    //     //     anchors.horizontalCenter: parent.horizontalCenter
    //     //     color: "transparent"
    //     //     border.color: "#1976d2"
    //     //     border.width: 2
    //     //
    //     //     Label {
    //     //         id: aoiBoxLabel
    //     //         anchors.centerIn: parent
    //     //         color: "#1565c0"
    //     //     }
    //     // }
    //
    //     // Label {
    //     //     id: centerLabel
    //     //     anchors.horizontalCenter: parent.horizontalCenter
    //     //     color: "#455a64"
    //     // }
    // }

    // FLOATING VERTICAL TOOLBOX
    Rectangle {
        id: floatingToolbox
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        anchors.rightMargin: 20

        width: 44
        height: toolboxLayout.implicitHeight + 16

        color: "#ffffff"
        border.color: "#cccccc"
        border.width: 1
        radius: 6

        ColumnLayout {
            id: toolboxLayout
            anchors.centerIn: parent
            spacing: 6

            Button {
                id: zoomInButton
                text: "＋"
                font.bold: true
                Layout.preferredWidth: 32
                Layout.preferredHeight: 32
                ToolTip.text: "Zoom In"
                ToolTip.visible: hovered
            }

            Button {
                id: zoomOutButton
                text: "－"
                font.bold: true
                Layout.preferredWidth: 32
                Layout.preferredHeight: 32
                ToolTip.text: "Zoom Out"
                ToolTip.visible: hovered
            }

            // Separator line between zoom and tools
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 1
                color: "#eeeeee"
            }

            Button {
                id: selectButton
                text: "⬈"
                font.pixelSize: 14
                Layout.preferredWidth: 32
                Layout.preferredHeight: 32
                checkable: true
                checked: true // Default tool
                ToolTip.text: "Select Tool"
                ToolTip.visible: hovered
            }

            Button {
                id: drawButton
                text: "✎"
                font.pixelSize: 16
                Layout.preferredWidth: 32
                Layout.preferredHeight: 32
                checkable: true
                ToolTip.text: "Draw AOI"
                ToolTip.visible: hovered
                visible: false
            }

            Button {
                id: tPointButton
                text: "T"
                font.bold: true
                font.pixelSize: 16
                Layout.preferredWidth: 32
                Layout.preferredHeight: 32
                checkable: true
                ToolTip.text: "Drop Zone (T-Point)"
                ToolTip.visible: hovered
            }
        }
    }
}