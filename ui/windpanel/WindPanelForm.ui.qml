import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Frame {
    id: root
    background: Item {}

    // Expose components for logic binding in WindPanel.qml
    property alias windModel: windModel
    property alias windListView: windListView
    property alias addRowButton: addRowButton
    property alias fetchButton: fetchButton

    ColumnLayout {
        // anchors.fill: parent
        width: parent.width
        height: parent.height
        spacing: 8

        Label {
            text: "WIND PROFILE"
            font.bold: true
            font.pixelSize: 16
            Layout.bottomMargin: 4
        }

        // Table Header
        RowLayout {
            Layout.fillWidth: true
            spacing: 6

            Label {
                text: "Altitude (ft)"
                Layout.fillWidth: true
                font.bold: true
                font.pixelSize: 12
                horizontalAlignment: Text.AlignHCenter
            }
            Label {
                text: "Speed"
                Layout.fillWidth: true
                font.bold: true
                font.pixelSize: 12
                horizontalAlignment: Text.AlignHCenter
            }
            Label {
                text: "Heading"
                Layout.fillWidth: true
                font.bold: true
                font.pixelSize: 12
                horizontalAlignment: Text.AlignHCenter
            }
            // Reserve space for the trash button column
            Item {
                Layout.preferredWidth: 28
            }
        }

        // Table Content (Scrollable List)
        ListView {
            id: windListView
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            spacing: 4

            ScrollBar.vertical: ScrollBar {
                policy: ScrollBar.AsNeeded
                active: true
            }

            model: ListModel {
                id: windModel
                // 5 default rows
                ListElement { altitude: "1000"; speed: "0.0"; heading: "0" }
                ListElement { altitude: "2000"; speed: "0.0"; heading: "0" }
                ListElement { altitude: "3000"; speed: "0.0"; heading: "0" }
                ListElement { altitude: "4000"; speed: "0.0"; heading: "0" }
                ListElement { altitude: "5000"; speed: "0.0"; heading: "0" }
            }

            // delegate: RowLayout {
            //     width: windListView.width
            //     spacing: 6
            //
            //     TextField {
            //         text: model.altitude
            //         Layout.fillWidth: true
            //         horizontalAlignment: TextInput.AlignHCenter
            //     }
            //     TextField {
            //         text: model.speed
            //         Layout.fillWidth: true
            //         horizontalAlignment: TextInput.AlignHCenter
            //     }
            //     TextField {
            //         text: model.heading
            //         Layout.fillWidth: true
            //         horizontalAlignment: TextInput.AlignHCenter
            //     }
            // }
        }

        // Action Buttons
        RowLayout {
            Layout.fillWidth: true
            spacing: 10
            Layout.topMargin: 4

            Button {
                id: addRowButton
                text: "＋ Add Row"
                Layout.fillWidth: true
            }

            Button {
                id: fetchButton
                text: "☁ Fetch Online"
                Layout.fillWidth: true
            }
        }
    }
}