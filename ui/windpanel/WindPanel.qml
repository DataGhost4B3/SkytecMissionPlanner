import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import handle

WindPanelForm {
    id: root

    windListView.delegate: RowLayout {
        width: windListView.width
        spacing: 6

        TextField {
            text: model.altitude
            Layout.fillWidth: true
            horizontalAlignment: TextInput.AlignHCenter
            // Critical: Saves manual edits back to the model so C++ gets the changes on Run
            onTextChanged: model.altitude = text
        }
        TextField {
            text: model.speed
            Layout.fillWidth: true
            horizontalAlignment: TextInput.AlignHCenter
            onTextChanged: model.speed = text
        }
        TextField {
            text: model.heading
            Layout.fillWidth: true
            horizontalAlignment: TextInput.AlignHCenter
            onTextChanged: model.heading = text
        }
        Button {
            text: "🗑"
            Layout.preferredWidth: 28
            Layout.preferredHeight: 28
            onClicked: {
                windModel.remove(index)
            }
        }
    }

    addRowButton.onClicked: {
        windModel.append({
            "altitude": "",
            "speed": "",
            "heading": ""
        })
        windListView.positionViewAtEnd()
    }

    fetchButton.onClicked: {
        let lat = parseFloat(window.latitudeField.text)
        let lon = parseFloat(window.longitudeField.text)
        let dateStr = window.dateField.text
        let timeStr = window.timeField.text

        if (isNaN(lat) || isNaN(lon) || dateStr.length !== 10 || timeStr.length !== 5) {
            window.showError("Please enter valid LZ Lat/Lon and Date (YYYY-MM-DD) / Time (HH:MM).")
            return
        }

        // Trigger the C++ fetch thread
        // Prompt for save first, then fetch
        window.requireSaveThen(function() {
            MissionController.fetchWindProfile(lat, lon, dateStr, timeStr)
        })
    }

    // Catch the returned C++ wind data and populate the UI table
    Connections {
        target: MissionController

        function onWindProfileFetched(windData) {
            windModel.clear()

            for (let i = 0; i < windData.length; i++) {
                windModel.append(windData[i])
            }

            windListView.positionViewAtBeginning()
        }
    }
}