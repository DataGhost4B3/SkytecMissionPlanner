import QtQuick
import QtQuick.Controls
import handle // Import the module where MissionController resides

MissionPanelForm {
    id: root

    generateButton.onClicked: {
        // Harvest all UI strings, convert to floats, and pack into a JS Object
        let missionData = {
            "lzLat": parseFloat(latitudeField.text) || 0.0,
            "lzLon": parseFloat(longitudeField.text) || 0.0,
            "lzElev": parseFloat(elevationField.text) || 0.0,
            "jumpAlt": parseFloat(jumpAltField.text) || 0.0,
            "openAlt": parseFloat(canopyAltField.text) || 0.0,
            "weight": parseFloat(weightField.text) || 0.0,
            "acVel": parseFloat(airSpeedField.text) || 0.0,
            "acHdg": parseFloat(airHeadingField.text) || 0.0,
            "canopyVel": parseFloat(canopyGroundSpeedField.text) || 0.0,
            "glideRatio": parseFloat(glideRatioField.text) || 0.0
        }

        // ADD THIS: Harvest the current state of the Wind Profile table
        let windArray = []
        for (let i = 0; i < window.windModel.count; i++) {
            let row = window.windModel.get(i)
            windArray.push({
                "altitude": parseFloat(row.altitude) || 0.0,
                "speed": parseFloat(row.speed) || 0.0,
                "heading": parseFloat(row.heading) || 0.0
            })
        }
        missionData["windProfile"] = windArray

        console.log("Sending parameters to physics engine...")
        MissionController.runMission(window.buildProjectJson())
        // MissionController.runMission(missionData)
    }

    airHeadingField.onTextChanged: {
        if (window.hasDz) {
            window.mapCanvas.gdalRenderer.updateFlightLineHeading(parseFloat(airHeadingField.text) || 0.0)
        }
    }
}