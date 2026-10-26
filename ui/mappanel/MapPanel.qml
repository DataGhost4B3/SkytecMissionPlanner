import QtQuick

MapPanelForm {
    // Implement Map Panel logic here
    // e.g., mapCheck.onCheckedChanged: console.log("Map checked:", mapCheck.checked)
    id: root

    // ADD THIS: Jump Logic
    function applyView() {
        let lat = parseFloat(centerLatField.text)
        let lon = parseFloat(centerLonField.text)
        let widthKm = parseFloat(viewWidthField.text)

        if (isNaN(lat) || isNaN(lon) || isNaN(widthKm)) {
            window.showError("Please enter valid numeric coordinates and width.")
            return
        }

        // [THE FIX]: Forcibly drop active focus so the fields can receive live updates again
        centerLatField.focus = false
        centerLonField.focus = false
        viewWidthField.focus = false

        // C++ updateMission automatically handles the jump and re-centers the UTM projection
        window.gdalRenderer.updateMission(lat, lon, widthKm, widthKm)
    }

    applyViewButton.onClicked: applyView()
    centerLatField.onAccepted: applyView()
    centerLonField.onAccepted: applyView()
    viewWidthField.onAccepted: applyView()

    downloadMap.onClicked: {
        downloadMap.text = "Downloading (Background)..."
        downloadMap.enabled = false
        mapCanvas.gdalRenderer.downloadMapOffline()
    }

    // Connections {
    //     target: mapCanvas.gdalRenderer
    //
    //     function onViewChanged() {
    //         sizeEstimateLabel.text = "Est. Cache Size: " + mapCanvas.gdalRenderer.estimateDownloadSize()
    //     }
    //
    //     function onDownloadComplete() {
    //         downloadMap.text = "Download map with checked features"
    //         downloadMap.enabled = true
    //     }
    // }
    Connections {
        target: window.gdalRenderer // Ensure the alias points to the global window

        function onViewChanged() {
            sizeEstimateLabel.text = "Est. Cache Size: " + window.gdalRenderer.estimateDownloadSize()

            // ADD THIS: Sync fields with actual Canvas state
            let center = window.gdalRenderer.getCenterLatLon()
            // Ensure fields are only updated if they are not actively being typed in
            if (!centerLatField.activeFocus && !centerLonField.activeFocus && !viewWidthField.activeFocus) {
                centerLatField.text = center.y.toFixed(5)
                centerLonField.text = center.x.toFixed(5)
                viewWidthField.text = window.gdalRenderer.getViewWidthKm().toFixed(2)
            }
        }

        function onDownloadComplete() {
            downloadMap.text = "Download map with checked features"
            downloadMap.enabled = true
        }
    }
}