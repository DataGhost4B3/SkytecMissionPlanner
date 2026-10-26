#pragma once
#include <QQuickPaintedItem>
#include <QImage>
#include <QtQml/qqmlregistration.h>
#include <gdal_priv.h> // Required for GDALDataset and GDAL types
#include <gdal_utils.h>
#include <ogr_spatialref.h>
#include <gdalwarper.h>
#include <QtConcurrent> // ADD THIS
#include <atomic>

class MapCanvasItem : public QQuickPaintedItem {
    Q_OBJECT
    QML_ELEMENT

public:
    MapCanvasItem(QQuickItem *parent = nullptr);
    ~MapCanvasItem() override; // Declared the destructor

    void paint(QPainter *painter) override;

    // Callables from your QML files
    Q_INVOKABLE void updateMission(double latitude, double longitude, double widthKm, double heightKm);
    Q_INVOKABLE void downloadMapOffline();
    // Q_INVOKABLE void loadOfflineMap(const QString& filePath);
    Q_INVOKABLE void zoomMap(double factor);
    Q_INVOKABLE void panMapPixels(int dx, int dy);
    Q_INVOKABLE void commitPan();
    // Explicitly declaring forceRefresh so QML can call it when the window resizes
    Q_INVOKABLE void forceRefresh();
    Q_INVOKABLE void drawEnvelope(const QVariantList& perimeterPoints);
    Q_INVOKABLE QString estimateDownloadSize();

    // ADD THESE: C++ Math functions for QML to call
    // Q_INVOKABLE QPointF screenToGeo(int mouseX, int mouseY);
    // Q_INVOKABLE QPointF getCenterGeo();
    // Q_INVOKABLE double getViewWidthKm();
    // Q_INVOKABLE void setViewWidthKm(double km);
    // ADD THESE THREE:
    Q_INVOKABLE QPointF screenToLatLon(int mouseX, int mouseY);
    Q_INVOKABLE QPointF getCenterLatLon();
    Q_INVOKABLE double getViewWidthKm();

    Q_INVOKABLE void setLzMarker(double lat, double lon);
    Q_INVOKABLE void setDzMarker(double lat, double lon, double heading);
    Q_INVOKABLE void clearDzMarker();
    Q_INVOKABLE void updateFlightLineHeading(double heading);

signals:
    // ADD THESE: Communicate state changes to QML
    void viewChanged();
    void downloadComplete();
    void progressUpdated(QString taskName, double percent);
    void errorReported(QString errorMessage);

private:
    void refreshMapBuffer(); // Moves heavy lifting out of paint()

    QImage m_mapBuffer;

    // ADD THESE: Tracks the temporary visual shift during a drag
    int m_visualPanX = 0;
    int m_visualPanY = 0;

    double m_centerLat = 28.6139;
    double m_centerLon = 77.2090;

    // Declared the missing GDAL and projection variables
    GDALDataset* m_dataset = nullptr;
    // GDALDataset* m_warpedDataset = nullptr; // ADD THIS
    double m_viewWidthMeters = 0.0;
    double m_viewHeightMeters = 0.0;
    double m_utmCenterX = 0.0;
    double m_utmCenterY = 0.0;

    double m_renderUtmCenterX = 0.0;
    double m_renderUtmCenterY = 0.0;
    double m_renderViewWidth = 0.0;
    double m_renderViewHeight = 0.0;

    double m_lzLat = 0.0;
    double m_lzLon = 0.0;
    bool m_showLz = false;

    double m_dzLat = 0.0;
    double m_dzLon = 0.0;
    double m_acHdg = 0.0;
    bool m_showDz = false;

    QString m_utmWkt; // Caches the UTM projection string

    QVariantList m_rawEnvelope; // Stores the Lat/Lon points so they can be redrawn on zoom/pan

    // ADD THIS: Safely tracks the active live-fetch thread so rapid panning cancels old threads
    std::atomic<int> m_fetchCounter{0};
};