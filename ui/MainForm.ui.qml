import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ApplicationWindow {
    id: window

    visible: true
    width: 1280
    height: 800
    minimumWidth: 900
    minimumHeight: 600

    title: "Mission Planner"
    color: "#f2f4f7"


    property alias lzNameField: missionPanel.lzNameField
    // Top-level action aliases
    property alias actionNew: actionNew
    property alias actionOpen: actionOpen
    property alias actionSave: actionSave
    property alias actionSaveAs: actionSaveAs
    property alias actionExit: actionExit
    property alias actionExportKml: actionExportKml // ADD THIS

    property alias actionSettings: actionSettings // ADD THIS

    property alias progressContainer: progressContainer
    property alias progressLabel: progressLabel
    property alias globalProgressBar: globalProgressBar
    property alias errorToast: errorToast

    // property alias generateButton: missionPanel.generateButton
    // ADD THESE THREE ALIASES:
    property alias mapCanvas: mapCanvas
    property alias gdalRenderer: mapCanvas.gdalRenderer
    property alias mapPanel: mapPanel

    property alias centerLatField: mapPanel.centerLatField
    property alias centerLonField: mapPanel.centerLonField
    property alias viewWidthField: mapPanel.viewWidthField

    property alias actionFitAoi: actionFitAoi
    property alias actionResetView: actionResetView
    property alias actionConfigureMaster: actionConfigureMaster
    property alias actionUpload: actionUpload
    property alias actionAbout: actionAbout
    property alias actionSetLightTheme: actionSetLightTheme
    property alias actionSetDarkTheme: actionSetDarkTheme

    property alias statusLabel: statusLabel
    property alias coordinateLabel: coordinateLabel
    property alias aboutDialog: aboutDialog

    // Aliases to handle logic in standard .qml files
    property alias filterInput: filterInput
    property alias clearFilterButton: clearFilterButton

    // Chained aliases from child components
    property alias latitudeField: missionPanel.latitudeField
    property alias longitudeField: missionPanel.longitudeField
    property alias dateField: missionPanel.dateField // ADD THIS
    property alias timeField: missionPanel.timeField // ADD THIS
    property alias generateButton: missionPanel.generateButton

    property alias elevationField: missionPanel.elevationField
    property alias jumpAltField: missionPanel.jumpAltField
    property alias canopyAltField: missionPanel.canopyAltField
    property alias weightField: missionPanel.weightField
    property alias airSpeedField: missionPanel.airSpeedField
    property alias airHeadingField: missionPanel.airHeadingField
    property alias canopyGroundSpeedField: missionPanel.canopyGroundSpeedField
    property alias glideRatioField: missionPanel.glideRatioField

    property alias windModel: windPanel.windModel // ADD THIS
    // property alias widthBox: missionPanel.widthBox
    // property alias heightBox: missionPanel.heightBox

    // property alias aoiBoxLabel: mapCanvas.aoiBoxLabel
    // property alias centerLabel: mapCanvas.centerLabel

    menuBar: MenuBar {
        Menu {
            title: "File"
            Action { id: actionNew; text: "New" }
            Action { id: actionOpen; text: "Open" }
            Action { id: actionSave; text: "Save" }
            Action { id: actionSaveAs; text: "Save As" }
            Action { id: actionExportKml; text: "Export KML..." } // ADD THIS
            Action { id: actionSettings; text: "Settings" }
            MenuSeparator {}
            Action { id: actionExit; text: "Exit" }
        }
        Menu {
            title: "Edit"
            Action { text: "Undo" }
            Action { text: "Redo" }
            Action { text: "Preferences" }
        }
        Menu {
            title: "View"
            Menu {
                title: "Theme"
                Action { id: actionSetLightTheme; text: "Light" }
                Action { id: actionSetDarkTheme; text: "Dark" }
            }
            MenuSeparator {}
            Action { id: actionFitAoi; text: "Fit AOI" }
            Action { id: actionResetView; text: "Reset View" }
        }
        Menu {
            title: "Pathfinder"
            Action { id: actionConfigureMaster; text: "Configure Master Pathfinder" }
            Action { id: actionUpload; text: "Upload to Master" }
        }
        Menu {
            title: "Help"
            Action { id: actionAbout; text: "About SkytecMP" }
        }
    }

    // 1. HEADER
    header: ToolBar {
        background: Rectangle {
            color: "#f6f6f6"
            border.color: "#dfdfdf"
            border.width: 1
        }

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 2
            anchors.rightMargin: 2

            Text {
                text: "CLI:"
                font.pixelSize: 11
                color: "#333333"
                Layout.leftMargin: 0
            }

            TextField {
                id: filterInput
                Layout.fillWidth: true
                Layout.preferredHeight: 28
                placeholderText: "Enter program parameters..."
                font.family: "Consolas" // Fixed invalid font fallback array
                font.pixelSize: 11
                leftPadding: 6
                verticalAlignment: TextInput.AlignVCenter

                background: Rectangle {
                    color: "#ffffff"
                    border.color: "#9c9c9c"
                    radius: 2
                }
            }

            Button {
                id: clearFilterButton
                text: "⎘"
                Layout.preferredWidth: 20
                Layout.preferredHeight: 20
                // onClicked removed: Handle this in the controlling .qml file
            }

            Button {
                text: "▼"
                Layout.preferredWidth: 20
                Layout.preferredHeight: 20
            }

            Button {
                text: "➔"
                Layout.preferredWidth: 20
                Layout.preferredHeight: 20
            }
        }
    }

    // 2. MAIN CONTENT (Moved out of the header)
    RowLayout {
        anchors.fill: parent
        spacing: 0

        MissionPanel {
            id: missionPanel
        }

        // MapCanvas {
        //     id: mapCanvas
        // }
        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            MapCanvas {
                id: mapCanvas
                Layout.fillWidth: true
                Layout.fillHeight: true
            }

            // Horizontal separator between Map Canvas and bottom panels
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 1
                color: "#cccccc"
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.fillHeight: false
                Layout.preferredHeight: 240 // Sets a fixed height for the bottom dock
                Layout.maximumHeight: 240   // Prevents the panels from crushing the map
                spacing: 20

                MapPanel {
                    id: mapPanel
                    Layout.fillWidth: true
                    Layout.fillHeight: true // Fills the 240px height
                    Layout.preferredWidth: 0
                }

                Rectangle {
                    Layout.preferredWidth: 1
                    Layout.fillHeight: true // Matches the 240px height
                    color: "#cccccc"
                }

                WindPanel {
                    id: windPanel
                    Layout.fillWidth: true
                    Layout.fillHeight: true // Fills the 240px height
                    Layout.preferredWidth: 0
                }

                // Item {
                //     Layout.fillWidth: true // Pushes the panels cleanly to the left
                // }
            }
        }
    }

    // 3. FOOTER
    footer: ToolBar {
        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 12
            anchors.rightMargin: 12

            Label {
                id: statusLabel
                text: "Ready"
                Layout.preferredWidth: 200 // Fixed width keeps the middle container centered
            }

            Item { Layout.fillWidth: true } // Left spacer

            // GLOBAL PROGRESS BAR CONTAINER
            RowLayout {
                id: progressContainer
                visible: false // Hidden until an operation starts
                spacing: 8

                Label {
                    id: progressLabel
                    text: "Processing..."
                    font.pixelSize: 12
                }

                ProgressBar {
                    id: globalProgressBar
                    value: 0.0
                    Layout.preferredWidth: 200
                }
            }

            Item { Layout.fillWidth: true } // Right spacer

            Label {
                id: coordinateLabel
                Layout.alignment: Qt.AlignRight
            }
        }
    }

    // 4. UPPER-RIGHT ERROR NOTIFICATION (TOAST)
    Popup {
        id: errorToast
        x: parent.width - width - 20 // Anchors to upper right
        y: 20
        width: 340
        height: 60
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        background: Rectangle {
            color: "#e53935" // Red alert color
            radius: 6
        }
        contentItem: RowLayout {
            Label { text: "⚠️"; font.pixelSize: 24; color: "white" }
            Label {
                id: errorToastText
                color: "white"
                font.bold: true
                wrapMode: Text.Wrap
                Layout.fillWidth: true
            }
        }
        Timer {
            id: toastTimer
            interval: 5000 // Auto-hides after 5 seconds
            onTriggered: errorToast.close()
        }
        // Callable function to trigger the popup
        function show(msg) {
            errorToastText.text = msg
            errorToast.open()
            toastTimer.restart()
        }
    }

    // 4. DIALOG (Moved out of the header)
    Dialog {
        id: aboutDialog
        title: "About Mission_Planner"
        modal: true
        anchors.centerIn: parent
        standardButtons: Dialog.Ok
        Label {
            text: "GeoMap — Projected AOI and map viewer"
        }
    }
}