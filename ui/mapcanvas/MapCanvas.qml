import QtQuick
import QtQuick.Controls
import ui

MapCanvasForm {
    id: root

    property alias gdalRenderer: gdalRenderer
    property bool mapInitialized: false

    // ADDED: Delays the GDAL fetch so the GUI window has time to appear on your screen
    Timer {
        id: startupTimer
        interval: 200
        running: false
        repeat: false
        onTriggered: {
            gdalRenderer.updateMission(28.6139, 77.2090, 50, 50)
        }
    }

    MapCanvasItem {
        id: gdalRenderer
        anchors.fill: parent
        z: -1

        // ADD THIS: High-performance QML Crosshair Overlay
        // Rectangle {
        //     width: 20; height: 2; color: "#e53935"
        //     anchors.centerIn: parent
        //     border.color: "white"; border.width: 1
        // }
        // Rectangle {
        //     width: 2; height: 20; color: "#e53935"
        //     anchors.centerIn: parent
        //     border.color: "white"; border.width: 1
        // }
        Item {
            id: crosshair

            property int crossLength: 20
            property int barThickness: 2
            // property color barColor: "#e53935"
            property color barColor: "blue"
            property color strokeColor: "blue"
            property int strokeWidth: 1

            width: crossLength + (strokeWidth * 2)
            height: crossLength + (strokeWidth * 2)
            anchors.centerIn: parent

            Canvas {
                anchors.fill: parent
                antialiasing: true

                onPaint: {
                    var ctx = getContext("2d");
                    ctx.reset();

                    var halfT = crosshair.barThickness / 2;
                    var halfL = crosshair.crossLength / 2;
                    var cx = width / 2;
                    var cy = height / 2;

                    ctx.beginPath();

                    // Top arm
                    ctx.moveTo(cx - halfT, cy - halfL);
                    ctx.lineTo(cx + halfT, cy - halfL);
                    ctx.lineTo(cx + halfT, cy - halfT);

                    // Right arm
                    ctx.lineTo(cx + halfL, cy - halfT);
                    ctx.lineTo(cx + halfL, cy + halfT);
                    ctx.lineTo(cx + halfT, cy + halfT);

                    // Bottom arm
                    ctx.lineTo(cx + halfT, cy + halfL);
                    ctx.lineTo(cx - halfT, cy + halfL);
                    ctx.lineTo(cx - halfT, cy + halfT);

                    // Left arm
                    ctx.lineTo(cx + halfL, cy - halfT); // Adjusted boundary overlap path
                    ctx.lineTo(cx - halfL, cy + halfT);
                    ctx.lineTo(cx - halfL, cy - halfT);
                    ctx.lineTo(cx - halfT, cy - halfT);

                    ctx.closePath();

                    ctx.fillStyle = crosshair.barColor;
                    ctx.fill();

                    ctx.strokeStyle = crosshair.strokeColor;
                    ctx.lineWidth = crosshair.strokeWidth;
                    ctx.lineJoin = "miter";
                    ctx.stroke();
                }
            }
        }

        MouseArea {
            anchors.fill: parent
            hoverEnabled: true

            property int lastX: 0
            property int lastY: 0

            enabled: selectButton.checked || tPointButton.checked

            onPressed: (mouse) => {
                forceActiveFocus()

                if (tPointButton.checked) {
                    let coord = gdalRenderer.screenToLatLon(mouse.x, mouse.y)
                    window.dzLat = coord.y // Y is Lat
                    window.dzLon = coord.x // X is Lon
                    window.hasDz = true
                    window.isDirty = true // Mark project as unsaved

                    let hdg = parseFloat(window.airHeadingField.text) || 0.0
                    gdalRenderer.setDzMarker(window.dzLat, window.dzLon, hdg)

                    selectButton.checked = true // Auto-revert back to the pan tool
                    return
                }

                lastX = mouse.x
                lastY = mouse.y
            }

            onPositionChanged: (mouse) => {
                // // ADD THIS: Instantly update the footer with real-world cursor Lat/Lon
                // let geo = gdalRenderer.screenToGeo(mouse.x, mouse.y)
                // window.coordinateLabel.text = "Lat: " + geo.x.toFixed(5) + " | Lon: " + geo.y.toFixed(5)

                if (pressed) {
                    let dx = mouse.x - lastX
                    let dy = mouse.y - lastY

                    gdalRenderer.panMapPixels(dx, dy)

                    lastX = mouse.x
                    lastY = mouse.y
                }

                // ADD THIS: Dynamic footer coordinate update
                let coord = gdalRenderer.screenToLatLon(mouse.x, mouse.y)
                // Assuming window is the global root ID for MainForm
                window.coordinateLabel.text = "Lat: " + coord.y.toFixed(5) + " | Lon: " + coord.x.toFixed(5)
            }

            // ADD THIS: Finalize the pan and load the new map data
            onReleased: {
                gdalRenderer.commitPan()
            }

            onWheel: (wheel) => {
                if (wheel.angleDelta.y > 0) {
                    gdalRenderer.zoomMap(0.8)
                } else {
                    gdalRenderer.zoomMap(1.25)
                }
            }
        }
    }

    ButtonGroup {
        buttons: [selectButton, drawButton, tPointButton]
    }

    zoomInButton.onClicked: gdalRenderer.zoomMap(0.8)
    zoomOutButton.onClicked: gdalRenderer.zoomMap(1.25)

    onWidthChanged: {
        if (width > 0 && height > 0) {
            if (!mapInitialized) {
                mapInitialized = true
                startupTimer.start() // Trigger the delay
            } else if (!startupTimer.running) {
                gdalRenderer.forceRefresh()
            }
        }
    }
}