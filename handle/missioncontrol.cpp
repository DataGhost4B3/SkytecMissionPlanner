#include "missioncontrol.hpp"
#include "powertrain.hpp" // Access the global params map
#include "engine.hpp"         // Access run_physics
#include <QDebug>
#include <QVariant>
#include <QFile>
#include <QTextStream>
#include <QUrl>
#include <QDateTime>
#include <QtConcurrent>
#include <cmath>

// ADD THESE:
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>
#include <QStandardPaths>
#include <QDir>

MissionController::MissionController(QObject *parent) : QObject(parent) {}

// void MissionController::runMission(const QVariantMap& missionData) {
//     // 1. Clear previous mission data
//     params.clear();
//     wind.clear(); // ADD THIS to reset the wind hashmap
//
//     // 2. Extract QML data into the C++ hashmap
//     params["LZ LAT"] = missionData["lzLat"].toDouble();
//     params["LZ LON"] = missionData["lzLon"].toDouble();
//     params["LZ ELEV"] = missionData["lzElev"].toDouble();
//     params["JUMP ALT"] = missionData["jumpAlt"].toDouble();
//     params["OPEN ALT"] = missionData["openAlt"].toDouble(); // Wait for missing UI ID
//     params["WT KG"] = missionData["weight"].toDouble();
//     params["AC VEL KTS"] = missionData["acVel"].toDouble();
//     params["AC HDG DEG"] = missionData["acHdg"].toDouble();
//     params["0-WIND CANOPY VEL"] = missionData["canopyVel"].toDouble();
//     params["GR"] = missionData["glideRatio"].toDouble();
//
//     // 3. Extract the manual QML table edits into the C++ wind hashmap
//     QVariantList windList = missionData["windProfile"].toList();
//     for (const QVariant& vRow : windList) {
//         QVariantMap row = vRow.toMap();
//         double alt = row["altitude"].toDouble();
//         double spd = row["speed"].toDouble();
//         double hdg = row["heading"].toDouble();
//
//         // Populate the global wind hashmap that powertrain uses
//         wind[alt] = std::make_pair(spd, hdg);
//     }
//
//     qDebug() << "Mission Parameters Loaded:" << params["LZ LAT"] << params["LZ LON"];
//
//     // 3. Trigger the physics engine
//     run_physics(path_lists, params, wind);
//
//     // Extract the very top Jump Points (Aircraft Release points) to draw the locus
//     QVariantList perimeter;
//     for (int i = 0; i < 360; ++i) {
//         if (!path_lists[i].empty()) {
//             QVariantMap pt;
//             pt["lat"] = path_lists[i][0].lat; // Index 0 is the exact jump point
//             pt["lon"] = path_lists[i][0].lon;
//             perimeter.append(pt);
//         }
//     }
//
//     emit envelopeCalculated(perimeter);
// }
void MissionController::runMission(const QVariantMap& missionData) {
    params.clear();
    wind.clear();

    params["LZ LAT"] = missionData["lzLat"].toDouble();
    params["LZ LON"] = missionData["lzLon"].toDouble();
    params["LZ ELEV"] = missionData["lzElev"].toDouble();
    params["JUMP ALT"] = missionData["jumpAlt"].toDouble();
    params["OPEN ALT"] = missionData["openAlt"].toDouble(); // CRITICAL: Ensure this is uncommented
    params["WT KG"] = missionData["weight"].toDouble();
    params["AC VEL KTS"] = missionData["acVel"].toDouble();
    params["AC HDG DEG"] = missionData["acHdg"].toDouble();
    params["0-WIND CANOPY VEL"] = missionData["canopyVel"].toDouble();
    params["GR"] = missionData["glideRatio"].toDouble();

    QVariantList windList = missionData["windProfile"].toList();
    for (const QVariant& vRow : windList) {
        QVariantMap row = vRow.toMap();
        double alt = row["altitude"].toDouble();
        double spd = row["speed"].toDouble();
        double hdg = row["heading"].toDouble();
        wind[alt] = std::make_pair(spd, hdg);
    }

    run_physics(path_lists, params, wind);

    // Extract the VERY TOP Jump Point (Aircraft Release point) to draw the locus
    QVariantList perimeter;
    for (int i = 0; i < 360; ++i) {
        if (!path_lists[i].empty()) {
            QVariantMap pt;
            pt["lat"] = path_lists[i][0].lat; // <--- Index 0 is the start of the parallel-shifted path
            pt["lon"] = path_lists[i][0].lon;
            perimeter.append(pt);
        }
    }

    emit envelopeCalculated(perimeter);
}

void MissionController::exportKml(const QString& filePath) {
    qDebug() << "KML Export triggered for:" << filePath;
    // TODO: Iterate through path_lists and generate KML XML structure
    QString localPath = QUrl(filePath).toLocalFile();
    if (localPath.isEmpty()) {
        localPath = filePath;
    }

    // 1. Load the external XML templates from the Qt Resource System
    QFile docFile(":/qt/qml/handle/kml_document.xml");
    QFile placemarkFile(":/qt/qml/handle/kml_placemark.xml");

    if (!docFile.open(QIODevice::ReadOnly | QIODevice::Text) ||
        !placemarkFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "Failed to load external KML XML templates.";
        return;
        }

    QString docTemplate = QTextStream(&docFile).readAll();
    QString placemarkTemplate = QTextStream(&placemarkFile).readAll();

    docFile.close();
    placemarkFile.close();

    // 2. Generate the internal Placemarks
    QString allPlacemarks;
    for (int i = 0; i < 360; ++i) {
        if (path_lists[i].empty()) continue;

        QString coordinates;
        for (const auto& pt : path_lists[i]) {
            // Build the coordinate block with strict spacing for clean XML output
            coordinates += QString("          %1,%2,%3\n")
                               .arg(pt.lon, 0, 'f', 6)
                               .arg(pt.lat, 0, 'f', 6)
                               .arg(pt.alt, 0, 'f', 2);
        }

        // Inject the heading (i) and coordinate list into the Placemark template
        allPlacemarks += placemarkTemplate.arg(i).arg(coordinates);
    }

    // 3. Inject all Placemarks into the main Document template
    QString finalKml = docTemplate.arg(allPlacemarks);

    // 4. Write the assembled KML to the user's hard drive
    QFile outFile(localPath);
    if (outFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&outFile);
        out << finalKml;
        outFile.close();
        qDebug() << "Successfully exported KML to:" << localPath;
    } else {
        qWarning() << "Could not open file for KML export:" << localPath;
    }
}

void MissionController::fetchWindProfile(double lat, double lon, const QString& date, const QString& time) {
    emit progressUpdated("Fetching Atmospheric Data...", 0.3);

    QtConcurrent::run([this, lat, lon, date, time]() {
        try {
            // 1. Generate current UTC date and exact hour for the API query
            // QDateTime now = QDateTime::currentDateTimeUtc();
            // QString date = now.toString("yyyy-MM-dd");
            // QString time = now.toString("HH:00");

            // 2. Execute your fetchwind.cpp HTTP request
            WindMap rawWind = fetchWind(lat, lon, date.toStdString(), time.toStdString());

            // 3. Clear the global powertrain hashmap to receive the new data
            wind.clear();
            QVariantList tableData;

            // 4. Convert Pressure (hPa) to Standard Altitude (Feet)
            for (const auto& [pressure, windData] : rawWind) {
                double speed = windData.first;
                double direction = windData.second;

                // Standard Barometric Formula: h = 145366.45 * (1 - (P / 1013.25)^0.190284)
                double altFeet = 145366.45 * (1.0 - std::pow(pressure / 1013.25, 0.190284));
                double roundedAlt = std::round(altFeet / 100.0) * 100.0; // Round to nearest 100ft

                // Populate the C++ Physics Hashmap
                wind[roundedAlt] = windData;

                // Populate the QML UI Row
                QVariantMap row;
                row["altitude"] = QString::number(roundedAlt, 'f', 0);
                row["speed"] = QString::number(speed, 'f', 1);
                row["heading"] = QString::number(direction, 'f', 0);
                tableData.append(row);
            }

            // 5. Sort the QML table data descending (Highest altitude at the top of the table)
            std::sort(tableData.begin(), tableData.end(), [](const QVariant& a, const QVariant& b) {
                return a.toMap()["altitude"].toDouble() > b.toMap()["altitude"].toDouble();
            });

            // 6. Push the updated data back to the QML Main Thread
            QMetaObject::invokeMethod(this, [this, tableData]() {
                emit windProfileFetched(tableData);
                emit progressUpdated("Wind Profile Downloaded", 1.0);
            }, Qt::QueuedConnection);

        } catch (const std::exception& e) {
            QMetaObject::invokeMethod(this, [this, e]() {
                emit errorReported(QString("Wind API Error: ") + e.what());
                emit progressUpdated("Failed", 1.0);
            }, Qt::QueuedConnection);
        }
    });
}

// --- PROJECT SERIALIZATION ---
void MissionController::saveProject(const QString& filePath, const QVariantMap& projectData) {
    QString localPath = QUrl(filePath).toLocalFile();
    if (localPath.isEmpty()) localPath = filePath;

    QFile file(localPath);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        // Automatically converts the QML dictionary to a formatted JSON file
        QJsonDocument doc(QJsonObject::fromVariantMap(projectData));
        file.write(doc.toJson());
        file.close();
        qDebug() << "Project saved to:" << localPath;
    } else {
        emit errorReported("Failed to save project file.");
    }
}

QVariantMap MissionController::loadProject(const QString& filePath) {
    QString localPath = QUrl(filePath).toLocalFile();
    if (localPath.isEmpty()) localPath = filePath;

    QFile file(localPath);
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
        file.close();
        return doc.object().toVariantMap(); // Returns JSON as a QML dictionary
    }
    emit errorReported("Failed to open project file.");
    return QVariantMap();
}

// --- SETTINGS MANAGEMENT ---
QString MissionController::getCachePath() {
    QSettings settings("Skytec", "MissionPlanner");
    // Defaults to OS AppData/map_cache if a user hasn't set one yet
    QString defaultPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/map_cache";
    return settings.value("cachePath", defaultPath).toString();
}

void MissionController::setCachePath(const QString& path) {
    QString localPath = QUrl(path).toLocalFile();
    if (localPath.isEmpty()) localPath = path;

    QSettings settings("Skytec", "MissionPlanner");
    settings.setValue("cachePath", localPath);

    // Ensure the new directory physically exists
    QDir().mkpath(localPath);
}