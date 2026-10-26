import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs // Required for FileDialog
// import Qt.labs.platform as Platform
import handle          // Required to access MissionController

MainForm {
    id: window


    property real dzLat: 0.0
    property real dzLon: 0.0
    property bool hasDz: false
    property bool isDirty: false
    property bool forceClose: false
    // --- APPLICATION STATE ---
    property string currentProjectPath: ""
    property var pendingCallback: null // Stores the intercepted action
    // property bool autoRunAfterFetch: false



    // --- MENU ACTIONS ---
    actionNew.onTriggered: { currentProjectPath = ""; statusLabel.text = "New project started."; window.isDirty = false} // Reset dirty flag for a blank slate }
    actionOpen.onTriggered: openProjectDialog.open()
    actionSave.onTriggered: {
        if (currentProjectPath === "") saveProjectDialog.open()
        else saveCurrentProject()
    }
    actionSaveAs.onTriggered: saveProjectDialog.open()
    actionSettings.onTriggered: {
        cachePathInput.text = MissionController.getCachePath()
        settingsDialog.open()
    }
    actionExit.onTriggered: window.close() //Qt.quit()
    actionExportKml.onTriggered: kmlFileDialog.open()

    // Intercept the native window closing event (e.g., clicking the red X)
    onClosing: (close_event) => {
        if (isDirty && !forceClose) {
            close_event.accepted = false // Stop the app from closing
            closeWarningDialog.open()    // Show the prompt instead
        }
    }

    // --- APPROACH B: THE SMOOTH INTERCEPT ---
    function requireSaveThen(callback) {
        if (currentProjectPath === "") {
            pendingCallback = callback
            saveProjectDialog.open() // Prompts save first
        } else {
            saveCurrentProject() // Auto-saves background progress
            callback() // Proceeds with Run or Fetch
        }
    }

    // --- JSON SERIALIZATION ASSEMBLERS ---
    // function buildProjectJson() {
    //     let windArray = []
    //     for (let i = 0; i < windModel.count; i++) {
    //         let r = windModel.get(i)
    //         windArray.push({"altitude": r.altitude, "speed": r.speed, "heading": r.heading})
    //     }
    //
    //     return {
    //         "lzLat": latitudeField.text,
    //         "lzLon": longitudeField.text,
    //         "date": dateField.text,
    //         "time": timeField.text,
    //         "mapCenterLat": centerLatField.text,
    //         "mapCenterLon": centerLonField.text,
    //         "mapWidth": viewWidthField.text,
    //         "windProfile": windArray
    //     }
    // }
    function buildProjectJson() {
        let windArray = []
        for (let i = 0; i < windModel.count; i++) {
            let r = windModel.get(i)
            windArray.push({"altitude": r.altitude, "speed": r.speed, "heading": r.heading})
        }

        return {
            "lzName": lzNameField.text, // <-- ADD THIS
            "lzLat": latitudeField.text,
            "lzLon": longitudeField.text,
            "lzElev": elevationField.text,
            "date": dateField.text,
            "time": timeField.text,
            "mapCenterLat": centerLatField.text,
            "mapCenterLon": centerLonField.text,
            "mapWidth": viewWidthField.text,
            "jumpAlt": jumpAltField.text,
            "openAlt": canopyAltField.text,
            "weight": weightField.text,
            "acVel": airSpeedField.text,
            "acHdg": airHeadingField.text,
            "canopyVel": canopyGroundSpeedField.text,
            "glideRatio": glideRatioField.text,
            "windProfile": windArray,
            // ADD THESE THREE:
            "dzLat": window.dzLat,
            "dzLon": window.dzLon,
            "hasDz": window.hasDz
        }
    }

    function saveCurrentProject() {
        MissionController.saveProject(currentProjectPath, buildProjectJson())
        statusLabel.text = "Project saved."
        window.isDirty = false // Reset dirty flag
    }

    // function loadProjectData(data) {
    //     latitudeField.text = data.lzLat || ""
    //     longitudeField.text = data.lzLon || ""
    //     dateField.text = data.date || ""
    //     timeField.text = data.time || ""
    //     centerLatField.text = data.mapCenterLat || ""
    //     centerLonField.text = data.mapCenterLon || ""
    //     viewWidthField.text = data.mapWidth || ""
    //
    //     windModel.clear()
    //     if (data.windProfile) {
    //         for (let i = 0; i < data.windProfile.length; i++) {
    //             windModel.append(data.windProfile[i])
    //         }
    //     }
    //
    //     // Snap the map to the loaded state
    //     mapPanel.applyView()
    // }
    function loadProjectData(data) {
        lzNameField.text = data.lzName || "" // <-- ADD THIS
        latitudeField.text = data.lzLat || ""
        longitudeField.text = data.lzLon || ""
        elevationField.text = data.lzElev || ""
        dateField.text = data.date || ""
        timeField.text = data.time || ""
        centerLatField.text = data.mapCenterLat || ""
        centerLonField.text = data.mapCenterLon || ""
        viewWidthField.text = data.mapWidth || ""
        jumpAltField.text = data.jumpAlt || ""
        canopyAltField.text = data.openAlt || ""
        weightField.text = data.weight || ""
        airSpeedField.text = data.acVel || ""
        airHeadingField.text = data.acHdg || ""
        canopyGroundSpeedField.text = data.canopyVel || ""
        glideRatioField.text = data.glideRatio || ""

        windModel.clear()
        if (data.windProfile) {
            for (let i = 0; i < data.windProfile.length; i++) {
                windModel.append(data.windProfile[i])
            }
        }
        mapPanel.applyView()
        // 1. Load and plot the DZ
        window.hasDz = data.hasDz || false
        if (window.hasDz) {
            window.dzLat = data.dzLat
            window.dzLon = data.dzLon
            gdalRenderer.setDzMarker(window.dzLat, window.dzLon, parseFloat(airHeadingField.text) || 0.0)
        } else {
            gdalRenderer.clearDzMarker()
        }

        // 2. Load and plot the LZ instantly upon opening a file
        let loadedLat = parseFloat(data.lzLat) || 0.0
        let loadedLon = parseFloat(data.lzLon) || 0.0
        if (loadedLat !== 0.0 && loadedLon !== 0.0) {
            gdalRenderer.setLzMarker(loadedLat, loadedLon)
        }

        window.isDirty = false // Reset dirty flag after a fresh load
    }

    function executeMission() {
        window.isDirty = true // Mark as dirty because a new HARP was generated

        // 1. Sync LZ coordinates to the Map Panel and jump the camera
        centerLatField.text = latitudeField.text
        centerLonField.text = longitudeField.text
        if (viewWidthField.text === "") viewWidthField.text = "25"
        mapPanel.applyView()

        // ADD THIS: Draw the LZ Marker
        gdalRenderer.setLzMarker(parseFloat(latitudeField.text), parseFloat(longitudeField.text))

        MissionController.runMission(buildProjectJson())
    }

    // --- DIALOGS ---
    // FileDialog {
    //     id: saveProjectDialog
    //     title: "Save Mission Project"
    //     fileMode: FileDialog.SaveFile
    //     nameFilters: ["Skytec Mission (*.skytec)"]
    //     defaultSuffix: "skytec"
    //     onAccepted: {
    //         currentProjectPath = selectedFile.toString()
    //         saveCurrentProject()
    //
    //         // Resume the intercepted action
    //         if (pendingCallback) {
    //             pendingCallback()
    //             pendingCallback = null
    //         }
    //     }
    //     onRejected: pendingCallback = null // Cancel action if they cancel save
    // }
    // Platform.FileDialog {
    //     id: saveProjectDialog
    //     title: "Save Mission Project"
    //     fileMode: Platform.FileDialog.SaveFile
    //     nameFilters: ["Skytec Mission (*.skytec)"]
    //     defaultSuffix: "skytec"
    //     onAccepted: {
    //         currentProjectPath = file.toString()
    //         saveCurrentProject()
    //
    //         // Resume the intercepted action (Run or Fetch Wind)
    //         if (pendingCallback) {
    //             pendingCallback()
    //             pendingCallback = null
    //         }
    //     }
    //     onRejected: pendingCallback = null
    // }
    FileDialog {
        id: saveProjectDialog
        title: "Save Mission Project"
        fileMode: FileDialog.SaveFile
        nameFilters: ["Skytec Mission (*.skytec)"]
        defaultSuffix: "skytec"
        onAccepted: {
            currentProjectPath = selectedFile.toString()
            saveCurrentProject()

            if (pendingCallback) {
                pendingCallback()
                pendingCallback = null
            }
        }
        onRejected: pendingCallback = null
    }

    // FileDialog {
    //     id: openProjectDialog
    //     title: "Open Mission Project"
    //     fileMode: FileDialog.OpenFile
    //     nameFilters: ["Skytec Mission (*.skytec)"]
    //     onAccepted: {
    //         currentProjectPath = selectedFile.toString()
    //         let data = MissionController.loadProject(currentProjectPath)
    //         loadProjectData(data)
    //         statusLabel.text = "Project loaded."
    //     }
    // }
    // Platform.FileDialog {
    //     id: openProjectDialog
    //     title: "Open Mission Project"
    //     fileMode: Platform.FileDialog.OpenFile
    //     nameFilters: ["Skytec Mission (*.skytec)"]
    //     onAccepted: {
    //         currentProjectPath = file.toString()
    //         let data = MissionController.loadProject(currentProjectPath)
    //         loadProjectData(data)
    //         statusLabel.text = "Project loaded."
    //     }
    // }
    FileDialog {
        id: openProjectDialog
        title: "Open Mission Project"
        fileMode: FileDialog.OpenFile
        nameFilters: ["Skytec Mission (*.skytec)"]
        onAccepted: {
            currentProjectPath = selectedFile.toString()
            let data = MissionController.loadProject(currentProjectPath)
            loadProjectData(data)
            statusLabel.text = "Project loaded."
            window.isDirty = false
        }
    }

    // Settings Menu Dialog
    Dialog {
        id: settingsDialog
        title: "Application Settings"
        anchors.centerIn: parent
        modal: true
        standardButtons: Dialog.Ok | Dialog.Cancel

        Column {
            spacing: 10
            Label { text: "Map Cache Directory:" }
            Row {
                spacing: 5
                TextField {
                    id: cachePathInput
                    width: 300
                    readOnly: true
                }
                Button {
                    text: "Browse..."
                    onClicked: cacheFolderDialog.open()
                }
            }
        }
        onAccepted: {
            MissionController.setCachePath(cachePathInput.text)
            window.showProgress("Cache path updated. Engine will use new path on next map interaction.", 1.0)
        }
    }

    Dialog {
        id: closeWarningDialog
        title: "Unsaved Changes"
        anchors.centerIn: parent
        modal: true
        standardButtons: Dialog.Save | Dialog.Discard | Dialog.Cancel

        Label {
            text: "You have unsaved changes. Do you want to save before exiting?"
            padding: 10
        }

        onAccepted: { // The user clicked "Save"
            window.requireSaveThen(function() {
                window.forceClose = true
                window.close()
            })
        }
        onDiscarded: { // The user clicked "Discard"
            window.forceClose = true
            window.close()
        }
        // If they click Cancel, the dialog simply closes and nothing happens.
    }

    FolderDialog {
        id: cacheFolderDialog
        title: "Select Map Cache Directory"
        onAccepted: cachePathInput.text = selectedFolder.toString()
    }
    // Platform.FolderDialog {
    //     id: cacheFolderDialog
    //     title: "Select Map Cache Directory"
    //     // Note: Qt.labs uses 'folder' instead of 'selectedFolder'
    //     onAccepted: cachePathInput.text = folder.toString()
    // }

    // [INJECT 1]: Verify the alias exists when the UI loads
    Component.onCompleted: {
        console.log("[DIAGNOSTIC 1] gdalRenderer resolves to:", gdalRenderer)
    }

    // State Variables
    property real latitude: 28.6139
    property real longitude: 77.2090
    property real aoiWidthKm: 50
    property real aoiHeightKm: 50

    // Dynamic UI Bindings
    latitudeField.text: String(latitude)
    longitudeField.text: String(longitude)
    // widthBox.value: aoiWidthKm
    // heightBox.value: aoiHeightKm

    // aoiBoxLabel.text: aoiWidthKm + " × " + aoiHeightKm + " km AOI"
    // centerLabel.text: "Center: " + Number(latitude).toFixed(5) + ", " + Number(longitude).toFixed(5)
    coordinateLabel.text: "Lat: " + Number(latitude).toFixed(5) + " | Lon: " + Number(longitude).toFixed(5)

    // Menu Action Handlers
    // actionNew.onTriggered: statusLabel.text = "New project"
    // actionOpen.onTriggered: statusLabel.text = "Open project"
    // actionSave.onTriggered: statusLabel.text = "Save project"
    // actionExit.onTriggered: Qt.quit()

    actionFitAoi.onTriggered: statusLabel.text = "Fit AOI"
    actionResetView.onTriggered: statusLabel.text = "Reset view"

    // actionGenerateAoi.onTriggered: generateButton.clicked()
    actionAbout.onTriggered: aboutDialog.open()

    // 1. Hook up the KML Export Menu Action
    // actionExportKml.onTriggered: kmlFileDialog.open()
    // Toolbar Button Handlers
    // toolNew.onClicked: statusLabel.text = "New project"
    // toolOpen.onClicked: statusLabel.text = "Open project"
    // toolSave.onClicked: statusLabel.text = "Save project"

    function showProgress(taskName, percent) {
        if (percent >= 1.0) {
            progressContainer.visible = false
            statusLabel.text = "Ready"
        } else {
            progressContainer.visible = true
            progressLabel.text = taskName
            globalProgressBar.value = percent
        }
    }

    function showError(message) {
        errorToast.show(message)
    }

    // --- C++ SIGNAL CONNECTIONS ---
    Connections {
        target: gdalRenderer // Now uses the direct alias from MainForm

        function onProgressUpdated(taskName, percent) {
            console.log("[DIAGNOSTIC 5] QML caught progress signal:", percent)
            showProgress(taskName, percent)
        }

        function onErrorReported(errorMessage) {
            showError(errorMessage)
        }

        // Catch the view changed signal to update the estimator label
        function onViewChanged() {
            mapPanel.sizeEstimateLabel.text = "Est. Cache Size: " + gdalRenderer.estimateDownloadSize()
        }

        // Reset the download button when the C++ thread finishes
        function onDownloadComplete() {
            mapPanel.downloadMap.text = "Download map with checked features"
            mapPanel.downloadMap.enabled = true
            showProgress("Complete", 1.0)
        }
    }

    // 2. Define the FileDialog
    // FileDialog {
    //     id: kmlFileDialog
    //     title: "Export Trajectories to KML"
    //     fileMode: FileDialog.SaveFile
    //     nameFilters: ["KML Files (*.kml)"]
    //     defaultSuffix: "kml"
    //     onAccepted: {
    //         // Pass the selected file URL to the C++ backend
    //         statusLabel.text = "Exporting KML..."
    //         MissionController.exportKml(selectedFile.toString())
    //         statusLabel.text = "KML Export Complete"
    //     }
    // }
    // Platform.FileDialog {
    //     id: kmlFileDialog
    //     title: "Export Trajectories to KML"
    //     fileMode: Platform.FileDialog.SaveFile
    //     nameFilters: ["KML Files (*.kml)"]
    //     defaultSuffix: "kml"
    //     onAccepted: {
    //         // Note: Qt.labs uses 'file' instead of 'selectedFile'
    //         statusLabel.text = "Exporting KML..."
    //         MissionController.exportKml(file.toString())
    //         statusLabel.text = "KML Export Complete"
    //     }
    // }
    FileDialog {
        id: kmlFileDialog
        title: "Export Trajectories to KML"
        fileMode: FileDialog.SaveFile
        nameFilters: ["KML Files (*.kml)"]
        defaultSuffix: "kml"
        onAccepted: {
            statusLabel.text = "Exporting KML..."
            MissionController.exportKml(selectedFile.toString())
            statusLabel.text = "KML Export Complete"
        }
    }

    // 3. Catch the C++ signal to draw the top-level HARP envelope on the map
    Connections {
        target: MissionController
        function onEnvelopeCalculated(perimeterPoints) {
            mapCanvas.gdalRenderer.drawEnvelope(perimeterPoints)
        }

        // Triggers the global progress bar in the footer when fetching wind/running physics
        function onProgressUpdated(taskName, percent) {
            showProgress(taskName, percent)
        }

        // Triggers the red alert toast if the Open-Meteo API fails
        function onErrorReported(errorMessage) {
            showError(errorMessage)
        }

        // Draws the HARP polygon when the physics engine finishes
        // function onEnvelopeCalculated(perimeterPoints) {
        //     gdalRenderer.drawEnvelope(perimeterPoints)
        // }
    }
    // Generate Button Logic
    // generateButton.onClicked: {
    //     if (latitudeField.acceptableInput && longitudeField.acceptableInput) {
    //         latitude = Number(latitudeField.text)
    //         longitude = Number(longitudeField.text)
    //         aoiWidthKm = widthBox.value
    //         aoiHeightKm = heightBox.value
    //
    //         statusLabel.text = "AOI parameters updated"
    //
    //         mapCanvas.gdalRenderer.updateMission(latitude, longitude, aoiWidthKm, aoiHeightKm)
    //     } else {
    //         statusLabel.text = "Enter valid coordinates"
    //     }
    // }
    generateButton.onClicked: {
        window.executeMission()
    }
}