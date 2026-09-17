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

// Engine Physics: Wind Interpolation with Ground Assumption
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

// 1. The Controller Bridge
class HarpController : public QObject {
    Q_OBJECT
public:
    explicit HarpController(QObject* parent = nullptr) : QObject(parent) {}

    Q_INVOKABLE bool generate(const QVariantMap& jp, const QVariantList& windList, const QString& fileUrl) {
        // Parse File URL safely cross-platform
        QString savePath = QUrl(fileUrl).toLocalFile();
        if (savePath.isEmpty()) return false;
        if (!savePath.endsWith(".kml", Qt::CaseInsensitive)) {
            savePath += ".kml";
        }
        // Extract UI Inputs
        double LZ_LAT = jp["lzLat"].toDouble();
        double LZ_LON = jp["lzLon"].toDouble();
        double lz_alt_m = jp["lzElev"].toDouble() * FT_TO_M;
        double jump_alt_m = jp["jumpAlt"].toDouble() * FT_TO_M;
        double open_alt_m = jp["openAlt"].toDouble() * FT_TO_M;
        double airv_ms = jp["airV"].toDouble() * KTS_TO_MS;
        double airh_deg = jp["airH"].toDouble();
        double gr = jp["gr"].toDouble();
        double descv_ms = jp["descV"].toDouble() * FT_TO_M;
        // wt is extracted but not actively calculating Vz yet in this iteration
        // double wt_kg = jp["wt"].toDouble();

        std::map<double, std::pair<double, double>> WIND;
        for (const QVariant& w : windList) {
            QVariantMap wm = w.toMap();
            WIND[wm["alt"].toDouble()] = {wm["spd"].toDouble(), wm["dir"].toDouble()};
        }
        std::cout << "WIND TABLE:\n";
        for (const auto& [alt, data] : WIND) {
            std::cout << "  " << alt << " ft: "
                      << data.first << " kt, "
                      << data.second << " deg\n";
        }

        // =========================================================================
        // ENGINE LOGIC: Parallel Shift Algorithm
        // =========================================================================
        double cos_lz_lat = std::cos(LZ_LAT * DEG_TO_RAD);
        double vz_ff_ms = 45.0;
        double dz = 5.0;

        // PHASE 1: Shared Freefall
        std::vector<CartPoint> shared_freefall_cartesian;
        double current_x = 0.0;
        double current_y = 0.0;

        double ac_u = -airv_ms * std::sin(airh_deg * DEG_TO_RAD);
        double ac_v = -airv_ms * std::cos(airh_deg * DEG_TO_RAD);

        for (double alt = jump_alt_m; alt >= open_alt_m; alt -= dz) {
            shared_freefall_cartesian.push_back({current_x, current_y, alt});

            auto [wind_kts, wind_deg] = interpolate_alt(WIND, alt * M_TO_FT, lz_alt_m * M_TO_FT);
          //   std::cout << "WIND TEST: " << wind_kts << " kt, "
          // << wind_deg << " deg\n";
            double wind_ms = wind_kts * KTS_TO_MS;
            double u_wind = -wind_ms * std::sin(wind_deg * DEG_TO_RAD);
            double v_wind = -wind_ms * std::cos(wind_deg * DEG_TO_RAD);

            ac_u *= 0.95; ac_v *= 0.95;

            double dt = dz / vz_ff_ms;
            current_x += (u_wind + ac_u) * dt;
            current_y += (v_wind + ac_v) * dt;
        }

        double canopy_open_x = current_x;
        double canopy_open_y = current_y;
        shared_freefall_cartesian.push_back({canopy_open_x, canopy_open_y, open_alt_m});

        // PHASE 2 & 3: Canopy Flight & Parallel Shift
        std::vector<std::vector<GeoPoint>> shifted_trajectories(720);
        double canopy_airspeed_ms = gr * descv_ms;

        std::cout << "LZ ALT: "
          << lz_alt_m * M_TO_FT
          << " ft\n";

        std::cout << "OPEN ALT: "
                  << open_alt_m * M_TO_FT
                  << " ft\n";
        for (int i = 0; i < 720; ++i) {
            double heading_rad = (i * 0.5) * DEG_TO_RAD;
            double u_canopy = canopy_airspeed_ms * std::sin(heading_rad);
            double v_canopy = canopy_airspeed_ms * std::cos(heading_rad);

            std::vector<CartPoint> full_path_cartesian = shared_freefall_cartesian;
            double cx = canopy_open_x;
            double cy = canopy_open_y;

            for (double alt = open_alt_m; alt > lz_alt_m; alt -= dz) {
                auto [wind_kts, wind_deg] = interpolate_alt(WIND, alt * M_TO_FT, lz_alt_m * M_TO_FT);
                // std::cout << "WIND TEST: " << wind_kts << " kt, " << wind_deg << " deg\n";
                double wind_ms = wind_kts * KTS_TO_MS;
                double u_wind = -wind_ms * std::sin(wind_deg * DEG_TO_RAD);
                double v_wind = -wind_ms * std::cos(wind_deg * DEG_TO_RAD);

                double dt = dz / descv_ms;
                cx += (u_wind + u_canopy) * dt;
                cy += (v_wind + v_canopy) * dt;

                full_path_cartesian.push_back({cx, cy, alt});
            }

            double shift_x = cx;
            double shift_y = cy;

            for (const auto& pt : full_path_cartesian) {
                double shifted_x = pt.x - shift_x;
                double shifted_y = pt.y - shift_y;

                double lon = LZ_LON + (shifted_x / (EARTH_RADIUS * cos_lz_lat)) * RAD_TO_DEG;
                double lat = LZ_LAT + (shifted_y / EARTH_RADIUS) * RAD_TO_DEG;

                shifted_trajectories[i].push_back({lat, lon, pt.alt_m * M_TO_FT});
            }
        }

        // =========================================================================
        // EXPORT TO KML
        // =========================================================================
        QFile outFile(savePath);
        if (!outFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
            std::cerr << "[ENGINE] Error: Could not open file for writing.\n";
            return false;
        }

        QTextStream out(&outFile);
        out.setLocale(QLocale::c());
        out.setRealNumberNotation(QTextStream::FixedNotation);
        out.setRealNumberPrecision(6);

        out << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
        out << "<kml xmlns=\"http://www.opengis.net/kml/2.2\">\n";
        out << "  <Document>\n";
        out << "    <name>HARP Trajectories</name>\n";
        out << "    <Style id=\"harpLine\">\n";
        out << "      <LineStyle>\n";
        out << "        <color>ff00ffff</color>\n";
        out << "        <width>2</width>\n";
        out << "      </LineStyle>\n";
        out << "    </Style>\n";
        out << "    <Style id=\"coverageSurface\">\n";
        out << "      <PolyStyle>\n";
        out << "        <color>7d00ff00</color>\n";
        out << "        <fill>1</fill>\n";
        out << "        <outline>0</outline>\n";
        out << "      </PolyStyle>\n";
        out << "    </Style>\n";
        out << "    <Style id=\"topPoint\">\n";
        out << "      <IconStyle>\n";
        out << "        <color>ff0000ff</color>\n";
        out << "        <scale>0.8</scale>\n";
        out << "      </IconStyle>\n";
        out << "    </Style>\n";
        out << "    <Style id=\"coneVertex\">\n";
        out << "      <IconStyle>\n";
        out << "        <color>ff0000ff</color>\n";
        out << "        <scale>1.0</scale>\n";
        out << "      </IconStyle>\n";
        out << "    </Style>\n";

        out << "    <Folder>\n";
        out << "      <name>Trajectories</name>\n";
        for (const auto& path : shifted_trajectories) {
            if (path.size() < 2) continue;
            out << "    <Placemark>\n";
            out << "      <styleUrl>#harpLine</styleUrl>\n";
            out << "      <LineString>\n";
            out << "        <extrude>0</extrude>\n";
            out << "        <tessellate>1</tessellate>\n";
            out << "        <altitudeMode>absolute</altitudeMode>\n";
            out << "        <coordinates>\n";
            for (const auto& pt : path) {
                double alt_m = pt.alt_ft * 0.3048;
                if (alt_m < 0.0) alt_m = 0.0;
                out << "          " << pt.lon << "," << pt.lat << "," << alt_m << "\n";
            }
            out << "        </coordinates>\n";
            out << "      </LineString>\n";
            out << "    </Placemark>\n";
        }

        out << "    </Folder>\n";

        out << "    <Placemark>\n";
        out << "      <name>Cone Vertex</name>\n";
        out << "      <styleUrl>#coneVertex</styleUrl>\n";
        out << "      <Point>\n";
        out << "        <altitudeMode>absolute</altitudeMode>\n";
        out << "        <coordinates>"
            << LZ_LON << "," << LZ_LAT << "," << lz_alt_m
            << "</coordinates>\n";
        out << "      </Point>\n";
        out << "    </Placemark>\n";

        // Green coverage surface connecting the top of all trajectories
        out << "    <Placemark>\n";
        out << "      <name>Trajectory Coverage</name>\n";
        out << "      <styleUrl>#coverageSurface</styleUrl>\n";
        out << "      <Polygon>\n";
        out << "        <altitudeMode>absolute</altitudeMode>\n";
        out << "        <outerBoundaryIs>\n";
        out << "          <LinearRing>\n";
        out << "            <coordinates>\n";

        for (const auto& path : shifted_trajectories) {
            if (path.empty()) continue;

            const auto& pt = path.front();

            double alt_m = pt.alt_ft * 0.3048;
            if (alt_m < 0.0) alt_m = 0.0;

            out << "              "
                << pt.lon << ","
                << pt.lat << ","
                << alt_m << "\n";
        }

        // Close the polygon
        if (!shifted_trajectories.empty() &&
            !shifted_trajectories.front().empty()) {

            const auto& pt = shifted_trajectories.front().front();

            double alt_m = pt.alt_ft * 0.3048;
            if (alt_m < 0.0) alt_m = 0.0;

            out << "              "
                << pt.lon << ","
                << pt.lat << ","
                << alt_m << "\n";
            }

        out << "            </coordinates>\n";
        out << "          </LinearRing>\n";
        out << "        </outerBoundaryIs>\n";
        out << "      </Polygon>\n";
        out << "    </Placemark>\n";

        out << "    <Placemark>\n";
        out << "      <name>Top Center</name>\n";
        out << "      <styleUrl>#topPoint</styleUrl>\n";
        out << "      <Point>\n";
        out << "        <altitudeMode>absolute</altitudeMode>\n";
        out << "        <coordinates>"
            << LZ_LON << "," << LZ_LAT << "," << jump_alt_m
            << "</coordinates>\n";
        out << "      </Point>\n";
        out << "    </Placemark>\n";

        out << "  </Document>\n";
        out << "</kml>\n";
        outFile.close();

        std::cout << "[ENGINE] Successfully wrote 720 KML Trajectories to: " << savePath.toStdString() << "\n";
        return true;
    }
};

// 2. The Embedded QML UI
// The R"qml(...)qml" tag explicitly tells CLion to treat this as QML, silencing C++ syntax errors.
const char* QML_SRC = R"qml(
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs

ApplicationWindow {
    title: "HARP Mission Planner"
    visible: true
    width: 850
    height: 500
    color: "#1e1e1e"

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
            var jp = {
                lzLat: Number(i_lat.text),
                lzLon: Number(i_lon.text),
                lzElev: Number(i_elev.text),
                jumpAlt: Number(i_jalt.text),
                openAlt: Number(i_oalt.text),
                airV: Number(i_airv.text),
                airH: Number(i_airh.text),
                gr: Number(i_gr.text),
                descV: Number(i_descv.text),
                wt: Number(i_wt.text)
            };

            var w = [];

            for (var i = 0; i < windModel.count; i++) {
                w.push({
                    alt: Number(windModel.get(i).alt),
                    spd: Number(windModel.get(i).spd),
                    dir: Number(windModel.get(i).dir)
                });
            }

            console.log("Wind rows:", windModel.count);

            for (var j = 0; j < windModel.count; j++) {
                console.log(
                    "Wind", j,
                    "alt:", windModel.get(j).alt,
                    "spd:", windModel.get(j).spd,
                    "dir:", windModel.get(j).dir
                );
            }

            if (HarpController.generate(jp, w, saveDialog.selectedFile)) {
                console.log("Successfully generated KML file!");
            }
        }
    }

    RowLayout {
        anchors.fill: parent
        anchors.margins: 10
        spacing: 15

        GroupBox {
            title: "Jump Parameters"
            Layout.fillHeight: true
            Layout.preferredWidth: 350

            GridLayout {
                columns: 2
                anchors.fill: parent

                Label {
                    text: "LZ Lat (deg):"
                    color: "white"
                }

                TextField {
                    id: i_lat
                    text: "29.503121"
                    Layout.fillWidth: true
                }

                Label {
                    text: "LZ Lon (deg):"
                    color: "white"
                }

                TextField {
                    id: i_lon
                    text: "78.006743"
                    Layout.fillWidth: true
                }

                Label {
                    text: "LZ Elev (ft MSL):"
                    color: "white"
                }

                TextField {
                    id: i_elev
                    text: "1968"
                    Layout.fillWidth: true
                }

                Label {
                    text: "Jump Alt (ft MSL):"
                    color: "white"
                }

                TextField {
                    id: i_jalt
                    text: "7000"
                    Layout.fillWidth: true
                }

                Label {
                    text: "Open Alt (ft MSL):"
                    color: "white"
                }

                TextField {
                    id: i_oalt
                    text: "5000"
                    Layout.fillWidth: true
                }

                Label {
                    text: "A/C Speed (KTS):"
                    color: "white"
                }

                TextField {
                    id: i_airv
                    text: "90"
                    Layout.fillWidth: true
                }

                Label {
                    text: "A/C Heading (deg):"
                    color: "white"
                }

                TextField {
                    id: i_airh
                    text: "25"
                    Layout.fillWidth: true
                }

                Label {
                    text: "Glide Ratio:"
                    color: "white"
                }

                TextField {
                    id: i_gr
                    text: "2.6"
                    Layout.fillWidth: true
                }

                Label {
                    text: "Descent (ft/s):"
                    color: "white"
                }

                TextField {
                    id: i_descv
                    text: "23"
                    Layout.fillWidth: true
                }

                Label {
                    text: "Mass (kg):"
                    color: "white"
                }

                TextField {
                    id: i_wt
                    text: "120"
                    Layout.fillWidth: true
                }
            }
        }

        ColumnLayout {
            Layout.fillHeight: true
            Layout.fillWidth: true

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
                        width: ListView.view.width
                        spacing: 5

                        TextField {
                            text: model.alt

                            onEditingFinished: {
                                windModel.setProperty(
                                    index,
                                    "alt",
                                    Number(text)
                                );
                            }

                            Layout.fillWidth: true
                        }

                        TextField {
                            text: model.spd

                            onEditingFinished: {
                                windModel.setProperty(
                                    index,
                                    "spd",
                                    Number(text)
                                );
                            }

                            Layout.fillWidth: true
                        }

                        TextField {
                            text: model.dir

                            onEditingFinished: {
                                windModel.setProperty(
                                    index,
                                    "dir",
                                    Number(text)
                                );
                            }

                            Layout.fillWidth: true
                        }

                        Button {
                            text: "X"

                            onClicked: {
                                windModel.remove(index);
                            }
                        }
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true

                Button {
                    text: "+ Add Layer"

                    onClicked: {
                        windModel.append({
                            alt: 0,
                            spd: 0,
                            dir: 0
                        });
                    }

                    Layout.fillWidth: true
                }

                Button {
                    text: "Clear Data"

                    onClicked: {
                        windModel.clear();
                    }

                    Layout.fillWidth: true
                }
            }

            Button {
                text: "GENERATE HARP (KML)"
                Layout.fillWidth: true
                Layout.preferredHeight: 50

                onClicked: {
                    saveDialog.open();
                }
            }
        }
    }
}
)qml";

// 3. Application Entry
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

// CRITICAL: This filename must exactly match the name of this C++ file.
// If you name this file main_2.cpp, this MUST be #include "main_2.moc"
#include "main.moc"