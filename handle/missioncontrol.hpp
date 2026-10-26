#pragma once
#include <QObject>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

class MissionController : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON // Allows QML to call it without instantiating it

public:
    explicit MissionController(QObject *parent = nullptr);

    // Receives a JSON-like object from QML
    Q_INVOKABLE void runMission(const QVariantMap& missionData);
    Q_INVOKABLE void exportKml(const QString& filePath);

    // ADD THIS: Bridges the Wind Panel to the fetchwind.cpp logic
    // Q_INVOKABLE void fetchWindProfile(double lat, double lon);
    Q_INVOKABLE void fetchWindProfile(double lat, double lon, const QString& date, const QString& time);

    Q_INVOKABLE void saveProject(const QString& filePath, const QVariantMap& projectData);
    Q_INVOKABLE QVariantMap loadProject(const QString& filePath);
    Q_INVOKABLE QString getCachePath();
    Q_INVOKABLE void setCachePath(const QString& path);
signals:
    void envelopeCalculated(QVariantList perimeterPoints);
    void progressUpdated(QString taskName, double percent); // Ensure these exist
    // ADD THIS: Sends the formatted table data back to QML
    void windProfileFetched(QVariantList windData);
    void errorReported(QString errorMessage);
};