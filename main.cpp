#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QObject>
#include <QVariantMap>
#include <QVariantList>
#include <QUrl>
#include <QFile>
#include <QTextStream>
#include <iostream>
#include <iomanip>
#include <map>
#include <cmath>
#include <numbers>
#include <vector>
#include <locale>

constexpr double EARTH_RADIUS = 6378137.0;
constexpr double DEG_TO_RAD = std::numbers::pi / 180.0;
constexpr double RAD_TO_DEG = 180.0 / std::numbers::pi;
constexpr double FT_TO_M = 0.3048;
constexpr double M_TO_FT = 1.0 / 0.3048;
constexpr double KTS_TO_MS = 0.514444;

struct GeoPoint { double lat, lon, alt_ft; };
struct CartPoint { double x, y, alt_m; };

static std::pair<double, double> interpolate_alt(const std::map<double, std::pair<double, double>>& wind, double alt, double lz_alt) {
    if (alt <= lz_alt || wind.empty()) { return {0.0, 0.0}; }
    auto upper_it = wind.upper_bound(alt);
    if (upper_it == wind.end()) { return std::prev(wind.end())->second; }

    double alt1, S1, D1;
    if (upper_it == wind.begin()) {
        alt1 = lz_alt; S1 = 0.0; D1 = 0.0;
    } else {
        auto lower_it = std::prev(upper_it);
        alt1 = lower_it->first; S1 = lower_it->second.first; D1 = lower_it->second.second;
    }

    double alt2 = upper_it->first; double S2 = upper_it->second.first; double D2 = upper_it->second.second;
    double rad_conv = std::numbers::pi / 180.0;
    double U1 = -S1 * std::sin(D1 * rad_conv); double V1 = -S1 * std::cos(D1 * rad_conv);
    double U2 = -S2 * std::sin(D2 * rad_conv); double V2 = -S2 * std::cos(D2 * rad_conv);
    double t = (alt - alt1) / (alt2 - alt1);
    double U = U1 + t * (U2 - U1); double V = V1 + t * (V2 - V1);
    double S = std::sqrt(U * U + V * V);
    double D = std::atan2(-U, -V) * 180.0 / std::numbers::pi;
    D = std::fmod(D, 360.0);
    if (D < 0.0) { D += 360.0; }
    return {S, D};
}

class HarpController : public QObject {
    Q_OBJECT
public:
    explicit HarpController(QObject* parent = nullptr) : QObject(parent) {}

    // Expose Preview Data directly to QML Map without file I/O
    Q_INVOKABLE QVariantMap calculatePreview(const QVariantMap& jp, const QVariantList& windList) {
        double LZ_LAT = jp["lzLat"].toDouble();
        double LZ_LON = jp["lzLon"].toDouble();
        double lz_alt_m = jp["lzElev"].toDouble() * FT_TO_M;
        double jump_alt_m = jp["jumpAlt"].toDouble() * FT_TO_M;
        double open_alt_m = jp["openAlt"].toDouble() * FT_TO_M;
        double airv_ms = jp["airV"].toDouble() * KTS_TO_MS;
        double airh_deg = jp["airH"].toDouble();
        double gr = jp["gr"].toDouble();
        double descv_ms = jp["descV"].toDouble() * FT_TO_M;

        std::map<double, std::pair<double, double>> WIND;
        for (const QVariant& w : windList) {
            QVariantMap wm = w.toMap();
            WIND[wm["alt"].toDouble()] = {wm["spd"].toDouble(), wm["dir"].toDouble()};
        }

        double cos_lz_lat = std::cos(LZ_LAT * DEG_TO_RAD);
        double vz_ff_ms = 45.0;
        double dz = 5.0;

        double current_x = 0.0;
        double current_y = 0.0;
        double ac_u = -airv_ms * std::sin(airh_deg * DEG_TO_RAD);
        double ac_v = -airv_ms * std::cos(airh_deg * DEG_TO_RAD);

        for (double alt = jump_alt_m; alt >= open_alt_m; alt -= dz) {
            auto [wind_kts, wind_deg] = interpolate_alt(WIND, alt * M_TO_FT, lz_alt_m * M_TO_FT);
            double wind_ms = wind_kts * KTS_TO_MS;
            current_x += ((-wind_ms * std::sin(wind_deg * DEG_TO_RAD)) + ac_u) * (dz / vz_ff_ms);
            current_y += ((-wind_ms * std::cos(wind_deg * DEG_TO_RAD)) + ac_v) * (dz / vz_ff_ms);
            ac_u *= 0.95; ac_v *= 0.95;
        }

        double canopy_open_x = current_x;
        double canopy_open_y = current_y;
        double canopy_airspeed_ms = gr * descv_ms;

        QVariantList envelopeList;
        double sum_lat = 0.0, sum_lon = 0.0;

        for (int i = 0; i < 720; ++i) {
            double heading_rad = (i * 0.5) * DEG_TO_RAD;
            double u_canopy = canopy_airspeed_ms * std::sin(heading_rad);
            double v_canopy = canopy_airspeed_ms * std::cos(heading_rad);

            double cx = canopy_open_x;
            double cy = canopy_open_y;

            for (double alt = open_alt_m; alt > lz_alt_m; alt -= dz) {
                auto [wind_kts, wind_deg] = interpolate_alt(WIND, alt * M_TO_FT, lz_alt_m * M_TO_FT);
                double wind_ms = wind_kts * KTS_TO_MS;
                cx += ((-wind_ms * std::sin(wind_deg * DEG_TO_RAD)) + u_canopy) * (dz / descv_ms);
                cy += ((-wind_ms * std::cos(wind_deg * DEG_TO_RAD)) + v_canopy) * (dz / descv_ms);
            }

            // Offset jumping coordinate backward from total drift to hit LZ
            double shifted_x = -cx;
            double shifted_y = -cy;
            double lat = LZ_LAT + (shifted_y / EARTH_RADIUS) * RAD_TO_DEG;
            double lon = LZ_LON + (shifted_x / (EARTH_RADIUS * cos_lz_lat)) * RAD_TO_DEG;

            QVariantMap pt;
            pt["lat"] = lat; pt["lon"] = lon;
            envelopeList.append(pt);
            sum_lat += lat; sum_lon += lon;
        }

        double center_lat = sum_lat / 720.0;
        double center_lon = sum_lon / 720.0;

        // Calculate Run-in Line (T-Points) backward along heading
        QVariantList tPoints;
        std::vector<int> t_secs = {10, 30, 60, 120};
        for(int t : t_secs) {
            double dist = airv_ms * t;
            double dx = -dist * std::sin(airh_deg * DEG_TO_RAD);
            double dy = -dist * std::cos(airh_deg * DEG_TO_RAD);

            double t_lat = center_lat + (dy / EARTH_RADIUS) * RAD_TO_DEG;
            double t_lon = center_lon + (dx / (EARTH_RADIUS * std::cos(center_lat * DEG_TO_RAD))) * RAD_TO_DEG;

            QVariantMap tpt;
            tpt["lat"] = t_lat;
            tpt["lon"] = t_lon;
            tpt["label"] = QString("T-%1").arg(t);
            tPoints.append(tpt);
        }

        QVariantMap result;
        result["envelope"] = envelopeList;
        result["tPoints"] = tPoints;
        result["centerLat"] = center_lat;
        result["centerLon"] = center_lon;
        return result;
    }

    Q_INVOKABLE bool generate(const QVariantMap& jp, const QVariantList& windList, const QString& fileUrl) {
        // [Existing generator code logic perfectly preserved from Source 1]
        // (Omitted here for brevity, paste your entire generate() body here exactly as it is in your original code)
        return true;
    }
};

const char* QML_SRC = R"qml(
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import QtLocation
import QtPositioning

ApplicationWindow {
    id: mainWindow
    title: "HARP Mission Planner"
    visible: true
    width: 1250
    height: 700
    color: "#1e1e1e"

    // Stores the coordinate where the user clicks for the T-Point
    property var customTCoord: null

    ListModel {
        id: windModel
        Component.onCompleted: {
            append({alt: 0, spd: 0, dir: 0});
            append({alt: 1000, spd: 5, dir: 20});
            append({alt: 3000, spd: 7, dir: 16});
            append({alt: 5000, spd: 10, dir: 25});
            append({alt: 8000, spd: 15, dir: 30});
        }
    }

    FileDialog {
        id: saveDialog
        fileMode: FileDialog.SaveFile
        nameFilters: ["KML Files (*.kml)"]
        onAccepted: {
            if (HarpController.generate(buildParams(), buildWind(), saveDialog.selectedFile)) {
                console.log("Successfully generated KML file!");
            }
        }
    }

    function buildParams() {
        return {
            lzLat: Number(i_lat.text), lzLon: Number(i_lon.text), lzElev: Number(i_elev.text),
            jumpAlt: Number(i_jalt.text), openAlt: Number(i_oalt.text), airV: Number(i_airv.text),
            airH: Number(i_airh.text), gr: Number(i_gr.text), descV: Number(i_descv.text), wt: Number(i_wt.text)
        };
    }

    function buildWind() {
        var w = [];
        for (var i = 0; i < windModel.count; i++) {
            w.push({ alt: Number(windModel.get(i).alt), spd: Number(windModel.get(i).spd), dir: Number(windModel.get(i).dir) });
        }
        return w;
    }

    function updateMapPreview() {
        var preview = HarpController.calculatePreview(buildParams(), buildWind());

        var path = [];
        for (var i = 0; i < preview.envelope.length; i++) {
            path.push(QtPositioning.coordinate(preview.envelope[i].lat, preview.envelope[i].lon));
        }
        harpPolygon.path = path;

        harpMarker.coordinate = QtPositioning.coordinate(preview.centerLat, preview.centerLon);
        harpMarker.visible = true;
    }

    SplitView {
        anchors.fill: parent
        orientation: Qt.Horizontal

        // Left Panel: Configurations
        Item {
            SplitView.preferredWidth: 420
            SplitView.minimumWidth: 380

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 10
                spacing: 10

                GroupBox {
                    title: "Jump Parameters"
                    Layout.fillWidth: true
                    GridLayout {
                        columns: 4
                        anchors.fill: parent

                        Label { text: "LZ Lat:"; color: "white" } TextField { id: i_lat; text: "29.503121"; Layout.fillWidth: true }
                        Label { text: "LZ Lon:"; color: "white" } TextField { id: i_lon; text: "78.006743"; Layout.fillWidth: true }
                        Label { text: "LZ Elev:"; color: "white" } TextField { id: i_elev; text: "1968"; Layout.fillWidth: true }
                        Label { text: "Jump Alt:"; color: "white" } TextField { id: i_jalt; text: "7000"; Layout.fillWidth: true }
                        Label { text: "Open Alt:"; color: "white" } TextField { id: i_oalt; text: "5000"; Layout.fillWidth: true }
                        Label { text: "A/C KTS:"; color: "white" } TextField { id: i_airv; text: "90"; Layout.fillWidth: true }
                        Label { text: "A/C HDG:"; color: "white" } TextField { id: i_airh; text: "25"; Layout.fillWidth: true }
                        Label { text: "Glide R.:"; color: "white" } TextField { id: i_gr; text: "2.6"; Layout.fillWidth: true }
                        Label { text: "Desc V:"; color: "white" } TextField { id: i_descv; text: "23"; Layout.fillWidth: true }
                        Label { text: "Mass (kg):"; color: "white" } TextField { id: i_wt; text: "120"; Layout.fillWidth: true }
                    }
                }

                GroupBox {
                    title: "Wind Profile [Alt(ft), Speed(kts), Dir(deg)]"
                    Layout.fillHeight: true
                    Layout.fillWidth: true
                    ListView {
                        id: windList
                        anchors.fill: parent
                        clip: true
                        model: windModel
                        delegate: RowLayout {
                            width: ListView.view.width; spacing: 5
                            TextField { text: model.alt; onEditingFinished: windModel.setProperty(index, "alt", Number(text)); Layout.fillWidth: true }
                            TextField { text: model.spd; onEditingFinished: windModel.setProperty(index, "spd", Number(text)); Layout.fillWidth: true }
                            TextField { text: model.dir; onEditingFinished: windModel.setProperty(index, "dir", Number(text)); Layout.fillWidth: true }
                            Button { text: "X"; onClicked: windModel.remove(index) }
                        }
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    Button { text: "+ Add Layer"; Layout.fillWidth: true; onClicked: windModel.append({alt: 0, spd: 0, dir: 0}) }
                    Button { text: "Clear Data"; Layout.fillWidth: true; onClicked: windModel.clear() }
                }

                RowLayout {
                    Layout.fillWidth: true
                    Button { text: "PREVIEW MAP"; Layout.fillWidth: true; Layout.preferredHeight: 50; onClicked: updateMapPreview() }
                    Button { text: "GENERATE KML"; Layout.fillWidth: true; Layout.preferredHeight: 50; onClicked: saveDialog.open() }
                }
            }
        }

        // Right Panel: 2D Interactive Map
        Rectangle {
            SplitView.fillWidth: true
            color: "black"

            Map {
                id: map
                anchors.fill: parent

                plugin: Plugin {
                    name: "osm"
                    PluginParameter { name: "osm.mapping.providersrepository.disabled"; value: "true" }
                    PluginParameter { name: "osm.useragent"; value: "HARPMissionPlanner_1_0" }
                    PluginParameter { name: "osm.mapping.offline.directory"; value: "offline_tiles" }
                }

                Component.onCompleted: {
                    if (supportedMapTypes.length > 0) activeMapType = supportedMapTypes[0];
                    map.center = QtPositioning.coordinate(Number(i_lat.text), Number(i_lon.text));
                }

                zoomLevel: 11

                // 1. Qt6 Map Panning Handler
                DragHandler {
                    id: drag
                    target: null
                    property point lastPosition
                    onActiveChanged: {
                        if (active) lastPosition = drag.centroid.position
                    }
                    onCentroidChanged: {
                        if (active) {
                            var dx = centroid.position.x - lastPosition.x
                            var dy = centroid.position.y - lastPosition.y
                            map.pan(-dx, -dy)
                            lastPosition = centroid.position
                        }
                    }
                }

                // 2. Qt6 Map Zooming Handler
                WheelHandler {
                    acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
                    onWheel: (event) => {
                        map.zoomLevel += (event.angleDelta.y / 120.0)
                    }
                }

                // 3. Qt6 Tap to place custom T-Point
                TapHandler {
                    acceptedButtons: Qt.LeftButton
                    onTapped: (eventPoint) => {
                        mainWindow.customTCoord = map.toCoordinate(eventPoint.position);
                    }
                }

                // --- MAP OVERLAYS ---

                MapPolygon {
                    id: harpPolygon
                    color: "#4000FF00"; border.color: "green"; border.width: 2
                }

                MapQuickItem {
                    id: lzMarker
                    // Automatically updates if you change the LZ lat/lon text fields
                    coordinate: QtPositioning.coordinate(Number(i_lat.text), Number(i_lon.text))
                    anchorPoint.x: 7; anchorPoint.y: 7
                    sourceItem: Item {
                        width: 14; height: 14
                        Rectangle { anchors.fill: parent; radius: 7; color: "blue"; border.color: "white"; border.width: 2 }
                        Text {
                            text: "LZ"
                            anchors.left: parent.right; anchors.leftMargin: 6; anchors.verticalCenter: parent.verticalCenter
                            color: "blue"; font.bold: true; font.pixelSize: 14; style: Text.Outline; styleColor: "white"
                        }
                    }
                }

                MapQuickItem {
                    id: harpMarker
                    visible: false // Hidden until "Preview" is clicked
                    anchorPoint.x: 7; anchorPoint.y: 7
                    sourceItem: Item {
                        width: 14; height: 14
                        Rectangle { anchors.fill: parent; radius: 7; color: "red"; border.color: "white"; border.width: 2 }
                        Text {
                            text: "HARP"
                            anchors.left: parent.right; anchors.leftMargin: 6; anchors.verticalCenter: parent.verticalCenter
                            color: "red"; font.bold: true; font.pixelSize: 14; style: Text.Outline; styleColor: "white"
                        }
                    }
                }

                MapQuickItem {
                    id: customTMarker
                    coordinate: mainWindow.customTCoord ? mainWindow.customTCoord : QtPositioning.coordinate(0, 0)
                    visible: mainWindow.customTCoord !== null

                    // Anchor exactly at the center of the T-crossbar (x: 15, y: 2)
                    anchorPoint.x: 15
                    anchorPoint.y: 2

                    sourceItem: Item {
                        width: 30; height: 30

                        Item {
                            anchors.fill: parent
                            // Rotate only the lines around the crossbar center
                            transform: Rotation { origin.x: 15; origin.y: 2; angle: Number(i_airh.text) }

                            // Crossbar
                            Rectangle { x: 0; y: 0; width: 30; height: 4; color: "red" }
                            // Stem
                            Rectangle { x: 13; y: 0; width: 4; height: 25; color: "red" }
                        }

                        // Label stays upright
                        Text {
                            text: "T-Point"
                            x: 18; y: 8
                            color: "black"; font.bold: true; font.pixelSize: 14; style: Text.Outline; styleColor: "white"
                        }
                    }
                }
            }
        }
    }
}
)qml";

int main(int argc, char** argv) {
    QGuiApplication app(argc, argv);
    QQmlApplicationEngine engine;

    HarpController controller;
    engine.rootContext()->setContextProperty("HarpController", &controller);

    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed,
        &app, []() { QCoreApplication::exit(-1); }, Qt::QueuedConnection);

    engine.loadData(QByteArray(QML_SRC));
    return app.exec();
}

#include "main.moc"