#include "mapcanvas.hpp"
#include <QPainter>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <cpl_vsi.h>
#include <cpl_conv.h>
#include <cmath>
#include <ogr_spatialref.h>
#include <gdalwarper.h>
#include <gdal_alg.h>
#include <QThread>
#include <algorithm> // Required for std::clamp

MapCanvasItem::MapCanvasItem(QQuickItem *parent) : QQuickPaintedItem(parent) {
    GDALAllRegister();
    CPLSetConfigOption("GDAL_CACHEMAX", "500");

    // ADDED: Critical Network Rules. OSM will block you and hang the thread without these.
    CPLSetConfigOption("GDAL_HTTP_USERAGENT", "MissionPlanner/1.0");
    CPLSetConfigOption("GDAL_HTTP_TIMEOUT", "5");
    CPLSetConfigOption("GDAL_HTTP_MAX_RETRY", "0");

    // Read the XML from the Qt Resource System
    QFile xmlFile(":/qt/qml/ui/mapcanvas/osm_config.xml");
    if (!xmlFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "Failed to load XML configuration from resources.";
        return;
    }

    // Inject the local cache path into the XML template
    QString rawXml = QTextStream(&xmlFile).readAll();
    // QString cachePath = QDir::currentPath() + "/map_cache";
    QSettings settings("Skytec", "MissionPlanner");
    QString defaultPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/map_cache";
    QString cachePath = settings.value("cachePath", defaultPath).toString();
    QDir().mkpath(cachePath); // Guarantee folder exists before GDAL runs
    QByteArray xmlConfig = rawXml.arg(cachePath).toUtf8();

    VSIFCloseL(VSIFileFromMemBuffer("/vsimem/osm_config.xml", (GByte*)xmlConfig.data(), xmlConfig.size(), FALSE));

    m_dataset = (GDALDataset*) GDALOpenEx("/vsimem/osm_config.xml", GDAL_OF_RASTER, nullptr, nullptr, nullptr);

    if (!m_dataset) {
        qWarning() << "Failed to open GDAL Web Map dataset.";
    }
}

// void MapCanvasItem::updateMission(double latitude, double longitude, double widthKm, double heightKm) {
//     m_centerLat = latitude;
//     m_centerLon = longitude;
//     m_viewWidthMeters = widthKm * 1000.0;
//     m_viewHeightMeters = heightKm * 1000.0;
//
//     if (!m_dataset) return;
//
//     int utmZone = static_cast<int>(std::floor((longitude + 180.0) / 6.0)) + 1;
//     bool isNorth = (latitude >= 0.0);
//
//     OGRSpatialReference latLonSrs, utmSrs;
//     latLonSrs.SetWellKnownGeogCS("WGS84");
//     latLonSrs.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
//     utmSrs.SetWellKnownGeogCS("WGS84");
//     utmSrs.SetUTM(utmZone, isNorth);
//
//     char *dstWkt = nullptr;
//     utmSrs.exportToWkt(&dstWkt);
//     m_utmWkt = QString(dstWkt);
//     CPLFree(dstWkt);
//
//     OGRCoordinateTransformation *coordTrans = OGRCreateCoordinateTransformation(&latLonSrs, &utmSrs);
//     double utmX = longitude;
//     double utmY = latitude;
//     if (coordTrans) {
//         coordTrans->Transform(1, &utmX, &utmY);
//         m_utmCenterX = utmX;
//         m_utmCenterY = utmY;
//         OCTDestroyCoordinateTransformation(coordTrans);
//     }
//
//     refreshMapBuffer(); // Generate the image in memory
//     emit viewChanged();
// }
void MapCanvasItem::updateMission(double latitude, double longitude, double widthKm, double heightKm) {
    // 1. Clamp Latitude to Web Mercator limits (prevents Infinity math crashes at the poles)
    latitude = std::clamp(latitude, -85.0511, 85.0511);

    // 2. Wrap Longitude cleanly so crossing the International Date Line works perfectly
    longitude = std::fmod(longitude, 360.0);
    if (longitude >= 180.0) longitude -= 360.0;
    else if (longitude < -180.0) longitude += 360.0;

    // 3. Clamp camera view limits to prevent impossible map fetch requests
    widthKm = std::clamp(widthKm, 0.05, 5000.0);
    heightKm = std::clamp(heightKm, 0.05, 5000.0);

    m_centerLat = latitude;
    m_centerLon = longitude;
    m_viewWidthMeters = widthKm * 1000.0;
    m_viewHeightMeters = heightKm * 1000.0;

    if (!m_dataset) return;

    // [THE FIX]: Safely clamp the UTM zone strictly between 1 and 60
    int utmZone = static_cast<int>(std::floor((longitude + 180.0) / 6.0)) + 1;
    utmZone = std::clamp(utmZone, 1, 60);
    bool isNorth = (latitude >= 0.0);

    OGRSpatialReference latLonSrs, utmSrs;
    latLonSrs.SetWellKnownGeogCS("WGS84");
    latLonSrs.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
    utmSrs.SetWellKnownGeogCS("WGS84");
    utmSrs.SetUTM(utmZone, isNorth);

    char *dstWkt = nullptr;
    utmSrs.exportToWkt(&dstWkt);
    m_utmWkt = QString(dstWkt);
    CPLFree(dstWkt);

    OGRCoordinateTransformation *coordTrans = OGRCreateCoordinateTransformation(&latLonSrs, &utmSrs);
    double utmX = longitude;
    double utmY = latitude;
    if (coordTrans) {
        coordTrans->Transform(1, &utmX, &utmY);
        m_utmCenterX = utmX;
        m_utmCenterY = utmY;
        OCTDestroyCoordinateTransformation(coordTrans);
    }

    refreshMapBuffer(); // Generate the image in memory
    emit viewChanged();
}

void MapCanvasItem::forceRefresh() {
    refreshMapBuffer();
}

// void MapCanvasItem::refreshMapBuffer() {
//     if (!m_dataset || m_viewWidthMeters <= 0 || m_viewHeightMeters <= 0 || m_utmWkt.isEmpty()) return;
//
//     int screenWidth = boundingRect().width();
//     int screenHeight = boundingRect().height();
//     if (screenWidth <= 0 || screenHeight <= 0) return;
//
//     // --- [THE FIX]: Force a 1:1 Aspect Ratio (Square Pixels) ---
//     // This dynamically recalculates the geographic height to match your physical window shape
//     double aspectRatio = static_cast<double>(screenWidth) / screenHeight;
//     m_viewHeightMeters = m_viewWidthMeters / aspectRatio;
//
//     GDALDriver *memDriver = GetGDALDriverManager()->GetDriverByName("MEM");
//     if (!memDriver) return;
//
//     qDebug() << "\n--- [DIAGNOSTIC START] ---";
//     qDebug() << "[1] Current UTM Center:" << m_utmCenterX << m_utmCenterY;
//
//     // --- 1. REVERSE MATH FIX ---
//     OGRSpatialReference latLonSrs, utmSrs;
//     latLonSrs.SetWellKnownGeogCS("WGS84");
//     latLonSrs.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
//
//     utmSrs.importFromWkt(m_utmWkt.toStdString().c_str());
//     utmSrs.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
//
//     OGRCoordinateTransformation *invTrans = OGRCreateCoordinateTransformation(&utmSrs, &latLonSrs);
//     double currentLon = m_utmCenterX;
//     double currentLat = m_utmCenterY;
//     if (invTrans) {
//         invTrans->Transform(1, &currentLon, &currentLat);
//         OCTDestroyCoordinateTransformation(invTrans);
//     }
//
//     qDebug() << "[2] Converted to Lat/Lon:" << currentLat << "(Lat) |" << currentLon << "(Lon)";
//
//     // Convert to Web Mercator
//     double webMercCenterX = currentLon * 20037508.34 / 180.0;
//     double latRad = currentLat * M_PI / 180.0;
//     double webMercCenterY = log(tan(M_PI / 4.0 + latRad / 2.0)) * 20037508.34 / M_PI;
//
//     qDebug() << "[3] Target Web Mercator X/Y:" << webMercCenterX << webMercCenterY;
//
//     double pad = 1.2;
//     double wmWidth = m_viewWidthMeters * pad;
//     double wmHeight = m_viewHeightMeters * pad;
//     double wmMinX = webMercCenterX - (wmWidth / 2.0);
//     double wmMaxY = webMercCenterY + (wmHeight / 2.0);
//
//     int wmPixWidth = screenWidth * pad;
//     int wmPixHeight = screenHeight * pad;
//
//     double wmsGeoTransform[6];
//     m_dataset->GetGeoTransform(wmsGeoTransform);
//     int xOff = static_cast<int>((wmMinX - wmsGeoTransform[0]) / wmsGeoTransform[1]);
//     int yOff = static_cast<int>((wmMaxY - wmsGeoTransform[3]) / wmsGeoTransform[5]);
//     int xSize = static_cast<int>(wmWidth / wmsGeoTransform[1]);
//     int ySize = static_cast<int>(std::abs(wmHeight / wmsGeoTransform[5]));
//
//     std::vector<GByte> rawBuffer(wmPixWidth * wmPixHeight * 3, 255);
//     int bands[3] = {1, 2, 3};
//
//     CPLErr fetchErr = m_dataset->RasterIO(GF_Read, xOff, yOff, xSize, ySize,
//                         rawBuffer.data(), wmPixWidth, wmPixHeight,
//                         GDT_Byte, 3, bands, 3, wmPixWidth * 3, 1);
//
//     if (fetchErr != CE_None) {
//         qWarning() << "[4] FATAL: Network fetch failed:" << CPLGetLastErrorMsg();
//         emit errorReported("Map Error: No internet connection and local cache is empty for this area.");
//     } else {
//         qDebug() << "[4] SUCCESS: Network chunk fetched.";
//     }
//
//     GDALDataset *srcDS = memDriver->Create("", wmPixWidth, wmPixHeight, 3, GDT_Byte, nullptr);
//     srcDS->RasterIO(GF_Write, 0, 0, wmPixWidth, wmPixHeight,
//                     rawBuffer.data(), wmPixWidth, wmPixHeight,
//                     GDT_Byte, 3, bands, 3, wmPixWidth * 3, 1);
//
//     double srcGeo[6] = { wmMinX, wmWidth/wmPixWidth, 0.0, wmMaxY, 0.0, -wmHeight/wmPixHeight };
//     srcDS->SetGeoTransform(srcGeo);
//
//     // [THE FIX]: Generate a real WKT string for EPSG:3857 instead of a raw abbreviation
//     OGRSpatialReference wmSrs;
//     wmSrs.importFromEPSG(3857);
//     char* wmWkt = nullptr;
//     wmSrs.exportToWkt(&wmWkt);
//     srcDS->SetProjection(wmWkt);
//     CPLFree(wmWkt);
//
//     // --- 2. WARP PHASE ---
//     // --- 2. WARP PHASE ---
//     GDALDataset *dstDS = memDriver->Create("", screenWidth, screenHeight, 3, GDT_Byte, nullptr);
//     QByteArray dstWktBytes = m_utmWkt.toUtf8();
//     dstDS->SetProjection(dstWktBytes.constData());
//
//     double utmMinX = m_utmCenterX - (m_viewWidthMeters / 2.0);
//     double utmMaxY = m_utmCenterY + (m_viewHeightMeters / 2.0);
//     double dstGeo[6] = { utmMinX, m_viewWidthMeters/screenWidth, 0.0, utmMaxY, 0.0, -m_viewHeightMeters/screenHeight };
//     dstDS->SetGeoTransform(dstGeo);
//
//     GDALWarpOptions *warpOpts = GDALCreateWarpOptions();
//     warpOpts->hSrcDS = srcDS;
//     warpOpts->hDstDS = dstDS;
//     warpOpts->eResampleAlg = GRA_Bilinear;
//     warpOpts->papszWarpOptions = CSLSetNameValue(warpOpts->papszWarpOptions, "INIT_DEST", "255,255,255");
//
//     // [THE FIX]: Create and assign the projection transformer function
//     warpOpts->pfnTransformer = GDALGenImgProjTransform;
//     warpOpts->pTransformerArg = GDALCreateGenImgProjTransformer2(srcDS, dstDS, nullptr);
//
//     GDALWarpOperation warpOp;
//     if (warpOp.Initialize(warpOpts) == CE_None) {
//         CPLErr warpErr = warpOp.ChunkAndWarpImage(0, 0, screenWidth, screenHeight);
//         qDebug() << "[5] Warp Operation Output:" << (warpErr == CE_None ? "SUCCESS" : "FAILED");
//     } else {
//         qDebug() << "[5] Warp Operation Failed to Initialize!";
//     }
//
//     // Clean up the transformer and options
//     if (warpOpts->pTransformerArg) {
//         GDALDestroyGenImgProjTransformer(warpOpts->pTransformerArg);
//     }
//     GDALDestroyWarpOptions(warpOpts);
//
//     // --- VISUAL DIAGNOSTIC DUMP ---
//     GDALDriver *tiffDriver = GetGDALDriverManager()->GetDriverByName("GTiff");
//     if (tiffDriver) {
//         GDALDataset* dump1 = tiffDriver->CreateCopy("debug_webmercator_fetch.tif", srcDS, FALSE, nullptr, nullptr, nullptr);
//         if (dump1) GDALClose(dump1);
//
//         GDALDataset* dump2 = tiffDriver->CreateCopy("debug_utm_warp.tif", dstDS, FALSE, nullptr, nullptr, nullptr);
//         if (dump2) GDALClose(dump2);
//         qDebug() << "[6] Wrote diagnostic .tif files to your cmake-build-debug folder.";
//     }
//
//     // --- 3. RENDER PHASE ---
//     std::vector<GByte> finalBuffer(screenWidth * screenHeight * 4, 255);
//     dstDS->RasterIO(GF_Read, 0, 0, screenWidth, screenHeight,
//                     finalBuffer.data(), screenWidth, screenHeight,
//                     GDT_Byte, 3, bands, 4, screenWidth * 4, 1);
//
//     m_mapBuffer = QImage(finalBuffer.data(), screenWidth, screenHeight, QImage::Format_RGBA8888).copy();
//     update();
//
//     GDALClose(srcDS);
//     GDALClose(dstDS);
//     qDebug() << "--- [DIAGNOSTIC END] ---\n";
// }

void MapCanvasItem::refreshMapBuffer() {
    if (!m_dataset || m_viewWidthMeters <= 0 || m_viewHeightMeters <= 0 || m_utmWkt.isEmpty()) return;

    int screenWidth = boundingRect().width();
    int screenHeight = boundingRect().height();
    if (screenWidth <= 0 || screenHeight <= 0) return;

    // Force 1:1 Aspect Ratio
    double aspectRatio = static_cast<double>(screenWidth) / screenHeight;
    m_viewHeightMeters = m_viewWidthMeters / aspectRatio;

    // 1. Thread Tracking (Cancels older threads if user pans quickly)
    int fetchId = ++m_fetchCounter;
    emit progressUpdated("Loading Map Data...", 0.0);

    // 2. Snapshot current state by value
    double viewW = m_viewWidthMeters;
    double viewH = m_viewHeightMeters;
    double utmCX = m_utmCenterX;
    double utmCY = m_utmCenterY;
    QString utmWkt = m_utmWkt;

    // Load XML config dynamically for the background thread
    QFile xmlFile(":/qt/qml/ui/mapcanvas/osm_config.xml");
    if (!xmlFile.open(QIODevice::ReadOnly | QIODevice::Text)) return;
    // QString threadXmlConfig = QTextStream(&xmlFile).readAll().arg(QDir::currentPath() + "/map_cache");
    QString rawXml = QTextStream(&xmlFile).readAll();

    QSettings settings("Skytec", "MissionPlanner");
    QString defaultPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/map_cache";
    QString cachePath = settings.value("cachePath", defaultPath).toString();
    QDir().mkpath(cachePath); // Guarantee folder exists before GDAL runs
    QString threadXmlConfig = rawXml.arg(cachePath);


    // 3. Launch Async Network Fetch
    QtConcurrent::run([this, fetchId, viewW, viewH, utmCX, utmCY, utmWkt, threadXmlConfig, screenWidth, screenHeight]() {
        GDALDriver *memDriver = GetGDALDriverManager()->GetDriverByName("MEM");
        QByteArray xmlBytes = threadXmlConfig.toUtf8();
        GDALDataset* threadDS = (GDALDataset*) GDALOpenEx(xmlBytes.constData(), GDAL_OF_RASTER, nullptr, nullptr, nullptr);

        if (!threadDS) return;

        // Coordinate Math
        OGRSpatialReference latLonSrs, utmSrs;
        latLonSrs.SetWellKnownGeogCS("WGS84");
        latLonSrs.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
        utmSrs.importFromWkt(utmWkt.toStdString().c_str());
        utmSrs.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);

        OGRCoordinateTransformation *invTrans = OGRCreateCoordinateTransformation(&utmSrs, &latLonSrs);
        double currentLon = utmCX;
        double currentLat = utmCY;
        if (invTrans) {
            invTrans->Transform(1, &currentLon, &currentLat);
            OCTDestroyCoordinateTransformation(invTrans);
        }

        double webMercCenterX = currentLon * 20037508.34 / 180.0;
        double latRad = currentLat * M_PI / 180.0;
        double webMercCenterY = log(tan(M_PI / 4.0 + latRad / 2.0)) * 20037508.34 / M_PI;

        // double pad = 1.2;
        // double wmWidth = viewW * pad;
        // double wmHeight = viewH * pad;
        // double wmMinX = webMercCenterX - (wmWidth / 2.0);
        // double wmMaxY = webMercCenterY + (wmHeight / 2.0);
        // [THE FIX]: Account for Web Mercator latitude stretch
        double scaleFactor = 1.0 / std::cos(latRad);
        double pad = 1.2;

        double wmWidth = viewW * scaleFactor * pad;
        double wmHeight = viewH * scaleFactor * pad;

        double wmMinX = webMercCenterX - (wmWidth / 2.0);
        double wmMaxY = webMercCenterY + (wmHeight / 2.0);

        int wmPixWidth = screenWidth * pad;
        int wmPixHeight = screenHeight * pad;

        double wmsGeoTransform[6];
        threadDS->GetGeoTransform(wmsGeoTransform);
        int xOff = static_cast<int>((wmMinX - wmsGeoTransform[0]) / wmsGeoTransform[1]);
        int yOff = static_cast<int>((wmMaxY - wmsGeoTransform[3]) / wmsGeoTransform[5]);
        int dsXSize = static_cast<int>(wmWidth / wmsGeoTransform[1]);
        int dsYSize = static_cast<int>(std::abs(wmHeight / wmsGeoTransform[5]));

        std::vector<GByte> rawBuffer(wmPixWidth * wmPixHeight * 3, 255);
        int bands[3] = {1, 2, 3};

        // 4. CHUNKED NETWORK FETCH (Provides real-time UI Progress)
        int strips = 4;
        bool fetchFailed = false;
        for (int i = 0; i < strips; ++i) {
            if (fetchId != m_fetchCounter) break; // Early abort if user panned again

            int yOffStrip = yOff + (i * dsYSize / strips);
            int ySizeStrip = dsYSize / strips;
            int destYOff = i * wmPixHeight / strips;
            int destYSize = wmPixHeight / strips;

            CPLErr err = threadDS->RasterIO(GF_Read, xOff, yOffStrip, dsXSize, ySizeStrip,
                                            rawBuffer.data() + (destYOff * wmPixWidth * 3),
                                            wmPixWidth, destYSize,
                                            GDT_Byte, 3, bands, 3, wmPixWidth * 3, 1);
            if (err != CE_None) {
                fetchFailed = true;
                break;
            }
            emit progressUpdated("Fetching Live Map...", static_cast<double>(i + 1) / strips);
        }

        GDALClose(threadDS);

        if (fetchId != m_fetchCounter) return; // Abort processing if stale

        if (fetchFailed) {
            emit errorReported("Map Error: No internet connection and cache is empty for this view.");
            emit progressUpdated("Failed", 1.0);
            return;
        }

        emit progressUpdated("Warping Projections...", 0.90);

        // 5. WARP PHASE
        GDALDataset *srcDS = memDriver->Create("", wmPixWidth, wmPixHeight, 3, GDT_Byte, nullptr);
        srcDS->RasterIO(GF_Write, 0, 0, wmPixWidth, wmPixHeight, rawBuffer.data(), wmPixWidth, wmPixHeight, GDT_Byte, 3, bands, 3, wmPixWidth * 3, 1);
        double srcGeo[6] = { wmMinX, wmWidth/wmPixWidth, 0.0, wmMaxY, 0.0, -wmHeight/wmPixHeight };
        srcDS->SetGeoTransform(srcGeo);

        OGRSpatialReference wmSrs;
        wmSrs.importFromEPSG(3857);
        char* wmWkt = nullptr;
        wmSrs.exportToWkt(&wmWkt);
        srcDS->SetProjection(wmWkt);
        CPLFree(wmWkt);

        GDALDataset *dstDS = memDriver->Create("", screenWidth, screenHeight, 3, GDT_Byte, nullptr);
        dstDS->SetProjection(utmWkt.toStdString().c_str());
        double utmMinX = utmCX - (viewW / 2.0);
        double utmMaxY = utmCY + (viewH / 2.0);
        double dstGeo[6] = { utmMinX, viewW/screenWidth, 0.0, utmMaxY, 0.0, -viewH/screenHeight };
        dstDS->SetGeoTransform(dstGeo);

        GDALWarpOptions *warpOpts = GDALCreateWarpOptions();
        warpOpts->hSrcDS = srcDS;
        warpOpts->hDstDS = dstDS;
        warpOpts->eResampleAlg = GRA_Bilinear;
        warpOpts->papszWarpOptions = CSLSetNameValue(warpOpts->papszWarpOptions, "INIT_DEST", "255,255,255");
        warpOpts->pfnTransformer = GDALGenImgProjTransform;
        warpOpts->pTransformerArg = GDALCreateGenImgProjTransformer2(srcDS, dstDS, nullptr);

        GDALWarpOperation warpOp;
        if (warpOp.Initialize(warpOpts) == CE_None) {
            warpOp.ChunkAndWarpImage(0, 0, screenWidth, screenHeight);
        }

        if (warpOpts->pTransformerArg) GDALDestroyGenImgProjTransformer(warpOpts->pTransformerArg);
        GDALDestroyWarpOptions(warpOpts);

        // 6. RENDER PHASE (Sync back to Main UI Thread)
        std::vector<GByte> finalBuffer(screenWidth * screenHeight * 4, 255);
        dstDS->RasterIO(GF_Read, 0, 0, screenWidth, screenHeight, finalBuffer.data(), screenWidth, screenHeight, GDT_Byte, 3, bands, 4, screenWidth * 4, 1);

        GDALClose(srcDS);
        GDALClose(dstDS);

        if (fetchId == m_fetchCounter) {
            // QMetaObject marshals the final image back to the main UI thread safely
            // QMetaObject::invokeMethod(this, [this, finalBuffer, screenWidth, screenHeight, fetchId]() {
            QMetaObject::invokeMethod(this, [this, finalBuffer, screenWidth, screenHeight, fetchId, viewW, viewH, utmCX, utmCY]() {
                if (fetchId == m_fetchCounter) {
                    m_mapBuffer = QImage(finalBuffer.data(), screenWidth, screenHeight, QImage::Format_RGBA8888).copy();

                    // LOCK the render coordinates to this specific image's boundaries
                    m_renderUtmCenterX = utmCX;
                    m_renderUtmCenterY = utmCY;
                    m_renderViewWidth = viewW;
                    m_renderViewHeight = viewH;

                    update();
                    emit progressUpdated("Ready", 1.0);
                }
            }, Qt::QueuedConnection);
        }
    });
}

// Missing Implementation: Zooming
void MapCanvasItem::zoomMap(double factor) {
    if (m_viewWidthMeters > 0 && m_viewHeightMeters > 0) {
        // Change the geographic size of the bounding box
        m_viewWidthMeters *= factor;
        m_viewHeightMeters *= factor;

        // Generate the new map image in memory, which will then call update()
        refreshMapBuffer();
        emit viewChanged();
    }
}

void MapCanvasItem::panMapPixels(int dx, int dy) {
    // Simply accumulate the pixel shift and tell Qt to redraw the screen
    m_visualPanX += dx;
    m_visualPanY += dy;
    update(); // Triggers paint() instantly, no GDAL math!
}

// void MapCanvasItem::commitPan() {
//     if (m_visualPanX == 0 && m_visualPanY == 0) return;
//     if (boundingRect().width() == 0 || boundingRect().height() == 0) return;
//
//     // 1. Calculate how many meters the accumulated pixel shift represents
//     double metersPerPixelX = m_viewWidthMeters / boundingRect().width();
//     double metersPerPixelY = m_viewHeightMeters / boundingRect().height();
//
//     // 2. Apply the shift to the real geographic coordinates
//     m_utmCenterX -= m_visualPanX * metersPerPixelX;
//     m_utmCenterY += m_visualPanY * metersPerPixelY; // + because Screen Y is inverted
//
//     // 3. Reset the visual shift back to zero
//     m_visualPanX = 0;
//     m_visualPanY = 0;
//
//     // 4. Now perform the heavy GDAL network fetch and warp (Executes just once)
//     refreshMapBuffer();
//     emit viewChanged();
// }
// void MapCanvasItem::commitPan() {
//     if (m_visualPanX == 0 && m_visualPanY == 0) return;
//     if (boundingRect().width() == 0 || boundingRect().height() == 0) return;
//
//     double mppX = m_viewWidthMeters / boundingRect().width();
//     double mppY = m_viewHeightMeters / boundingRect().height();
//
//     double newUtmX = m_utmCenterX - m_visualPanX * mppX;
//     double newUtmY = m_utmCenterY + m_visualPanY * mppY;
//
//     // Reset visual pan buffer
//     m_visualPanX = 0;
//     m_visualPanY = 0;
//
//     // Convert the new UTM center back to Lat/Lon
//     OGRSpatialReference latLonSrs, utmSrs;
//     latLonSrs.SetWellKnownGeogCS("WGS84");
//     latLonSrs.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
//     utmSrs.importFromWkt(m_utmWkt.toStdString().c_str());
//     utmSrs.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
//
//     OGRCoordinateTransformation *invTrans = OGRCreateCoordinateTransformation(&utmSrs, &latLonSrs);
//     if (invTrans) {
//         double newLon = newUtmX;
//         double newLat = newUtmY;
//         invTrans->Transform(1, &newLon, &newLat);
//         OCTDestroyCoordinateTransformation(invTrans);
//
//         // Feed it into updateMission. This instantly recalculates the UTM zone,
//         // rebuilds the projection strings, and cleanly fetches the new map data.
//         updateMission(newLat, newLon, m_viewWidthMeters / 1000.0, m_viewHeightMeters / 1000.0);
//     }
// }
void MapCanvasItem::commitPan() {
    if (m_visualPanX == 0 && m_visualPanY == 0) return;
    if (boundingRect().width() == 0 || boundingRect().height() == 0) return;

    double mppX = m_viewWidthMeters / boundingRect().width();
    double mppY = m_viewHeightMeters / boundingRect().height();

    double newUtmX = m_utmCenterX - m_visualPanX * mppX;
    double newUtmY = m_utmCenterY + m_visualPanY * mppY;

    m_visualPanX = 0;
    m_visualPanY = 0;

    OGRSpatialReference latLonSrs, utmSrs;
    latLonSrs.SetWellKnownGeogCS("WGS84");
    latLonSrs.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
    utmSrs.importFromWkt(m_utmWkt.toStdString().c_str());
    utmSrs.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);

    OGRCoordinateTransformation *invTrans = OGRCreateCoordinateTransformation(&utmSrs, &latLonSrs);
    if (invTrans) {
        double newLon = newUtmX;
        double newLat = newUtmY;
        invTrans->Transform(1, &newLon, &newLat);
        OCTDestroyCoordinateTransformation(invTrans);

        updateMission(newLat, newLon, m_viewWidthMeters / 1000.0, m_viewHeightMeters / 1000.0);
    } else {
        // [THE FIX]: If GDAL fails to transform, abort the pan and just refresh the map in place
        refreshMapBuffer();
        emit viewChanged();
    }
}

// QString MapCanvasItem::estimateDownloadSize() {
//     if (m_viewWidthMeters <= 0 || m_viewHeightMeters <= 0) return "0 KB";
//
//     // Assuming 4 meters per pixel resolution target
//     double targetXPixels = m_viewWidthMeters / 4.0;
//     double targetYPixels = m_viewHeightMeters / 4.0;
//     double totalPixels = targetXPixels * targetYPixels;
//
//     // A typical compressed OSM PNG tile averages ~0.4 bytes per pixel
//     double estimatedBytes = totalPixels * 0.4;
//     double estimatedMB = estimatedBytes / (1024.0 * 1024.0);
//
//     if (estimatedMB < 1.0) {
//         return QString::number(estimatedMB * 1024.0, 'f', 1) + " KB";
//     }
//     return QString::number(estimatedMB, 'f', 1) + " MB";
// }
// QString MapCanvasItem::estimateDownloadSize() {
//     if (m_viewWidthMeters <= 0 || m_viewHeightMeters <= 0) return "0 KB";
//
//     int screenW = boundingRect().width();
//     if (screenW <= 0) return "0 KB";
//
//     double currentRes = m_viewWidthMeters / screenW;
//     const double MIN_RES = 1.0; // Max zoom depth set to 1 meter per pixel
//
//     double totalPixels = 0;
//
//     // Mathematically calculates exact pixel counts for every zoom level down to 1m/px
//     for (double r = currentRes; r >= MIN_RES; r /= 2.0) {
//         totalPixels += (m_viewWidthMeters / r) * (m_viewHeightMeters / r);
//     }
//
//     double estimatedBytes = totalPixels * 0.4;
//     double estimatedMB = estimatedBytes / (1024.0 * 1024.0);
//
//     if (estimatedMB < 1.0) {
//         return QString::number(estimatedMB * 1024.0, 'f', 1) + " KB";
//     }
//     return QString::number(estimatedMB, 'f', 1) + " MB";
// }
QString MapCanvasItem::estimateDownloadSize() {
    if (m_viewWidthMeters <= 0 || m_viewHeightMeters <= 0 || m_utmWkt.isEmpty()) return "0 KB";

    int screenW = boundingRect().width();
    if (screenW <= 0) return "0 KB";

    // 1. Calculate the exact padded Web Mercator geometry
    double latRad = m_centerLat * M_PI / 180.0;
    double scaleFactor = 1.0 / std::cos(latRad);
    double pad = 3.0; // Matches the 3x padding applied in the downloader

    double wmWidth = m_viewWidthMeters * scaleFactor * pad;
    double wmHeight = m_viewHeightMeters * scaleFactor * pad;

    // 2. Adjust base resolution to account for the padded width
    double currentRes = wmWidth / (screenW * pad);
    const double MIN_RES = 1.0;

    double totalPixels = 0;
    for (double r = currentRes; r >= MIN_RES; r /= 2.0) {
        totalPixels += (wmWidth / r) * (wmHeight / r);
    }

    double estimatedBytes = totalPixels * 0.4;
    double estimatedMB = estimatedBytes / (1024.0 * 1024.0);

    if (estimatedMB < 1.0) {
        return QString::number(estimatedMB * 1024.0, 'f', 1) + " KB";
    }
    return QString::number(estimatedMB, 'f', 1) + " MB";
}

// void MapCanvasItem::downloadMapOffline() {
//     if (!m_dataset || m_viewWidthMeters <= 0 || m_viewHeightMeters <= 0) return;
//
//     // [INJECT 3]: Verify main thread entry
//     qDebug() << "[DIAGNOSTIC 2] downloadMapOffline() triggered by QML.";
//
//     qDebug() << "Starting offline cache population in background thread...";
//
//     // 1. Load a fresh XML configuration dynamically for the background thread
//     QFile xmlFile(":/qt/qml/ui/mapcanvas/osm_config.xml");
//     if (!xmlFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
//         qWarning() << "Failed to load XML configuration for background thread.";
//         return;
//     }
//     QString rawXml = QTextStream(&xmlFile).readAll();
//     QString cachePath = QDir::currentPath() + "/map_cache";
//
//     // Generate the final XML string
//     QString threadXmlConfig = rawXml.arg(cachePath);
//
//     // 1. Snapshot the current view parameters by value so they don't change if the user pans during download
//     double viewW = m_viewWidthMeters;
//     double viewH = m_viewHeightMeters;
//     double utmCX = m_utmCenterX;
//     double utmCY = m_utmCenterY;
//     QString utmWkt = m_utmWkt;
//
//     // 2. Launch the background thread
//     QtConcurrent::run([this, viewW, viewH, utmCX, utmCY, utmWkt, threadXmlConfig]() {
//         // [INJECT 4]: Verify thread start
//         qDebug() << "[DIAGNOSTIC 3] QtConcurrent background thread started.";
//         QByteArray xmlBytes = threadXmlConfig.toUtf8();
//         // // Open a thread-local dataset so we don't collide with the UI thread's memory
//         // GDALDataset* threadDS = (GDALDataset*) GDALOpenEx("/vsimem/osm_config.xml", GDAL_OF_RASTER, nullptr, nullptr, nullptr);
//         // Pass the raw XML string directly instead of using a /vsimem/ file
//         GDALDataset* threadDS = (GDALDataset*) GDALOpenEx(xmlBytes.constData(), GDAL_OF_RASTER, nullptr, nullptr, nullptr);
//         if (!threadDS) {
//             // [INJECT 5]: Check for silent early abort
//             qDebug() << "[DIAGNOSTIC 4 - FATAL] threadDS failed to open. Thread aborting silently.";
//             emit errorReported("Map Error: Failed to initialize background downloader.");
//             return;
//         }
//
//         qDebug() << "[DIAGNOSTIC 4] threadDS opened. Beginning chunk loop...";
//
//         OGRSpatialReference latLonSrs, utmSrs;
//         latLonSrs.SetWellKnownGeogCS("WGS84");
//         latLonSrs.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
//         utmSrs.importFromWkt(utmWkt.toStdString().c_str());
//         utmSrs.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
//
//         OGRCoordinateTransformation *invTrans = OGRCreateCoordinateTransformation(&utmSrs, &latLonSrs);
//         double currentLon = utmCX;
//         double currentLat = utmCY;
//         if (invTrans) {
//             invTrans->Transform(1, &currentLon, &currentLat);
//             OCTDestroyCoordinateTransformation(invTrans);
//         }
//
//         double webMercCenterX = currentLon * 20037508.34 / 180.0;
//         double latRad = currentLat * M_PI / 180.0;
//         double webMercCenterY = log(tan(M_PI / 4.0 + latRad / 2.0)) * 20037508.34 / M_PI;
//
//         double wmMinX = webMercCenterX - (viewW / 2.0);
//         double wmMaxY = webMercCenterY + (viewH / 2.0);
//
//         double wmsGeo[6];
//         threadDS->GetGeoTransform(wmsGeo);
//
//         int xOff = static_cast<int>((wmMinX - wmsGeo[0]) / wmsGeo[1]);
//         int yOff = static_cast<int>((wmMaxY - wmsGeo[3]) / wmsGeo[5]);
//         int dsXSize = static_cast<int>(viewW / wmsGeo[1]);
//         int dsYSize = static_cast<int>(std::abs(viewH / wmsGeo[5]));
//
//         int targetXPixels = static_cast<int>(viewW / 4.0);
//         int targetYPixels = static_cast<int>(viewH / 4.0);
//
//         const int CHUNK = 1024;
//         std::vector<GByte> dummyBuffer(CHUNK * CHUNK * 3);
//         int bands[3] = {1, 2, 3};
//
//         // Calculate total chunks for the progress bar
//         int numChunksX = std::ceil(targetXPixels / static_cast<double>(CHUNK));
//         int numChunksY = std::ceil(targetYPixels / static_cast<double>(CHUNK));
//         int totalChunks = numChunksX * numChunksY;
//         if (totalChunks == 0) totalChunks = 1; // Prevent division by zero
//         int currentChunk = 0;
//
//         for (int y = 0; y < targetYPixels; y += CHUNK) {
//             for (int x = 0; x < targetXPixels; x += CHUNK) {
//                 int readXSize = std::min(CHUNK, targetXPixels - x);
//                 int readYSize = std::min(CHUNK, targetYPixels - y);
//
//                 int srcXOff = xOff + (x * dsXSize / targetXPixels);
//                 int srcYOff = yOff + (y * dsYSize / targetYPixels);
//                 int srcXSize = readXSize * dsXSize / targetXPixels;
//                 int srcYSize = readYSize * dsYSize / targetYPixels;
//
//                 threadDS->RasterIO(GF_Read, srcXOff, srcYOff, srcXSize, srcYSize,
//                                     dummyBuffer.data(), readXSize, readYSize,
//                                     GDT_Byte, 3, bands, 3, readXSize * 3, 1);
//
//                 // ADD THIS: Wait 100 milliseconds between chunks to bypass OSM anti-bot throttling
//                 QThread::msleep(100);
//                 // Update the progress bar
//                 currentChunk++;
//                 double pct = static_cast<double>(currentChunk) / totalChunks;
//                 emit progressUpdated("Downloading Map Cache...", pct);
//             }
//         }
//
//         GDALClose(threadDS);
//         qDebug() << "Background download complete.";
//         // Hide the progress bar
//         emit progressUpdated("Downloading Map Cache...", 1.0);
//         emit downloadComplete(); // Notify the UI thread
//     });
// }
// void MapCanvasItem::downloadMapOffline() {
//     if (!m_dataset || m_viewWidthMeters <= 0 || m_viewHeightMeters <= 0) return;
//
//     int screenW = boundingRect().width();
//     int screenH = boundingRect().height();
//     if (screenW <= 0 || screenH <= 0) return;
//
//     QFile xmlFile(":/qt/qml/ui/mapcanvas/osm_config.xml");
//     if (!xmlFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
//         emit errorReported("Map Error: Failed to load XML configuration.");
//         return;
//     }
//     QString threadXmlConfig = QTextStream(&xmlFile).readAll().arg(QDir::currentPath() + "/map_cache");
//
//     double viewW = m_viewWidthMeters;
//     double viewH = m_viewHeightMeters;
//     double utmCX = m_utmCenterX;
//     double utmCY = m_utmCenterY;
//     QString utmWkt = m_utmWkt;
//
//     QtConcurrent::run([this, viewW, viewH, utmCX, utmCY, utmWkt, threadXmlConfig, screenW, screenH]() {
//         QByteArray xmlBytes = threadXmlConfig.toUtf8();
//         GDALDataset* threadDS = (GDALDataset*) GDALOpenEx(xmlBytes.constData(), GDAL_OF_RASTER, nullptr, nullptr, nullptr);
//         if (!threadDS) {
//             emit errorReported("Map Error: Failed to initialize background downloader.");
//             return;
//         }
//
//         OGRSpatialReference latLonSrs, utmSrs;
//         latLonSrs.SetWellKnownGeogCS("WGS84");
//         latLonSrs.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
//         utmSrs.importFromWkt(utmWkt.toStdString().c_str());
//         utmSrs.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
//
//         OGRCoordinateTransformation *invTrans = OGRCreateCoordinateTransformation(&utmSrs, &latLonSrs);
//         double currentLon = utmCX;
//         double currentLat = utmCY;
//         if (invTrans) {
//             invTrans->Transform(1, &currentLon, &currentLat);
//             OCTDestroyCoordinateTransformation(invTrans);
//         }
//
//         double webMercCenterX = currentLon * 20037508.34 / 180.0;
//         double latRad = currentLat * M_PI / 180.0;
//         double webMercCenterY = log(tan(M_PI / 4.0 + latRad / 2.0)) * 20037508.34 / M_PI;
//         // double wmMinX = webMercCenterX - (viewW / 2.0);
//         // double wmMaxY = webMercCenterY + (viewH / 2.0);
//         // [THE FIX]: Ensure the offline pyramid fetch accounts for latitude stretch
//         double scaleFactor = 1.0 / std::cos(latRad);
//
//         double wmMinX = webMercCenterX - (viewW * scaleFactor / 2.0);
//         double wmMaxY = webMercCenterY + (viewH * scaleFactor / 2.0);
//
//         double wmsGeo[6];
//         threadDS->GetGeoTransform(wmsGeo);
//
//         int xOff = static_cast<int>((wmMinX - wmsGeo[0]) / wmsGeo[1]);
//         int yOff = static_cast<int>((wmMaxY - wmsGeo[3]) / wmsGeo[5]);
//         int dsXSize = static_cast<int>(viewW / wmsGeo[1]);
//         int dsYSize = static_cast<int>(std::abs(viewH / wmsGeo[5]));
//
//         const int CHUNK = 512;
//         std::vector<GByte> dummyBuffer(CHUNK * CHUNK * 3);
//         int bands[3] = {1, 2, 3};
//
//         double currentRes = viewW / screenW;
//         const double MIN_RES = 1.0;
//
//         // 1. Pre-calculate total chunks for accurate progress tracking
//         int totalChunks = 0;
//         for (double r = currentRes; r >= MIN_RES; r /= 2.0) {
//             int targetXPixels = viewW / r;
//             int targetYPixels = viewH / r;
//             totalChunks += std::ceil(static_cast<double>(targetXPixels) / CHUNK) *
//                            std::ceil(static_cast<double>(targetYPixels) / CHUNK);
//         }
//         if (totalChunks == 0) totalChunks = 1;
//
//         int currentChunk = 0;
//         int layer = 1;
//
//         // 2. Execute the Pyramid Fetch
//         for (double r = currentRes; r >= MIN_RES; r /= 2.0) {
//             int targetXPixels = viewW / r;
//             int targetYPixels = viewH / r;
//
//             for (int y = 0; y < targetYPixels; y += CHUNK) {
//                 for (int x = 0; x < targetXPixels; x += CHUNK) {
//                     int readXSize = std::min(CHUNK, targetXPixels - x);
//                     int readYSize = std::min(CHUNK, targetYPixels - y);
//
//                     int srcXOff = xOff + (x * dsXSize / targetXPixels);
//                     int srcYOff = yOff + (y * dsYSize / targetYPixels);
//                     int srcXSize = readXSize * dsXSize / targetXPixels;
//                     int srcYSize = readYSize * dsYSize / targetYPixels;
//
//                     threadDS->RasterIO(GF_Read, srcXOff, srcYOff, srcXSize, srcYSize,
//                                         dummyBuffer.data(), readXSize, readYSize,
//                                         GDT_Byte, 3, bands, 3, readXSize * 3, 1);
//
//                     QThread::msleep(50); // Paced to avoid IP ban
//
//                     currentChunk++;
//                     double pct = static_cast<double>(currentChunk) / totalChunks;
//                     emit progressUpdated(QString("Caching Map Depth %1...").arg(layer), pct);
//                 }
//             }
//             layer++;
//         }
//
//         GDALClose(threadDS);
//         emit progressUpdated("Offline Cache Complete", 1.0);
//         emit downloadComplete();
//     });
// }
void MapCanvasItem::downloadMapOffline() {
    if (!m_dataset || m_viewWidthMeters <= 0 || m_viewHeightMeters <= 0) return;

    int screenW = boundingRect().width();
    int screenH = boundingRect().height();
    if (screenW <= 0 || screenH <= 0) return;

    QFile xmlFile(":/qt/qml/ui/mapcanvas/osm_config.xml");
    if (!xmlFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        emit errorReported("Map Error: Failed to load XML configuration.");
        return;
    }
    // QString threadXmlConfig = QTextStream(&xmlFile).readAll().arg(QDir::currentPath() + "/map_cache");
    QString rawXml = QTextStream(&xmlFile).readAll();

    // Dynamically retrieve the cache path from settings
    QSettings settings("Skytec", "MissionPlanner");
    QString defaultPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/map_cache";
    QString cachePath = settings.value("cachePath", defaultPath).toString();
    QDir().mkpath(cachePath); // Guarantee folder exists before GDAL runs

    // Generate the final XML string
    QString threadXmlConfig = rawXml.arg(cachePath);

    double viewW = m_viewWidthMeters;
    double viewH = m_viewHeightMeters;
    double utmCX = m_utmCenterX;
    double utmCY = m_utmCenterY;
    QString utmWkt = m_utmWkt;

    QtConcurrent::run([this, viewW, viewH, utmCX, utmCY, utmWkt, threadXmlConfig, screenW, screenH]() {
        QByteArray xmlBytes = threadXmlConfig.toUtf8();
        GDALDataset* threadDS = (GDALDataset*) GDALOpenEx(xmlBytes.constData(), GDAL_OF_RASTER, nullptr, nullptr, nullptr);
        if (!threadDS) {
            emit errorReported("Map Error: Failed to initialize background downloader.");
            return;
        }

        OGRSpatialReference latLonSrs, utmSrs;
        latLonSrs.SetWellKnownGeogCS("WGS84");
        latLonSrs.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
        utmSrs.importFromWkt(utmWkt.toStdString().c_str());
        utmSrs.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);

        OGRCoordinateTransformation *invTrans = OGRCreateCoordinateTransformation(&utmSrs, &latLonSrs);
        double currentLon = utmCX;
        double currentLat = utmCY;
        if (invTrans) {
            invTrans->Transform(1, &currentLon, &currentLat);
            OCTDestroyCoordinateTransformation(invTrans);
        }

        double webMercCenterX = currentLon * 20037508.34 / 180.0;
        double latRad = currentLat * M_PI / 180.0;
        double webMercCenterY = log(tan(M_PI / 4.0 + latRad / 2.0)) * 20037508.34 / M_PI;

        // 1. CALCULATE THE OFFLINE PANNING BUFFER
        double scaleFactor = 1.0 / std::cos(latRad);
        double pad = 3.0; // Downloads 1 full screen in every direction surrounding your current view

        double wmWidth = viewW * scaleFactor * pad;
        double wmHeight = viewH * scaleFactor * pad;

        double wmMinX = webMercCenterX - (wmWidth / 2.0);
        double wmMaxY = webMercCenterY + (wmHeight / 2.0);

        double wmsGeo[6];
        threadDS->GetGeoTransform(wmsGeo);

        int xOff = static_cast<int>((wmMinX - wmsGeo[0]) / wmsGeo[1]);
        int yOff = static_cast<int>((wmMaxY - wmsGeo[3]) / wmsGeo[5]);
        int dsXSize = static_cast<int>(wmWidth / wmsGeo[1]);
        int dsYSize = static_cast<int>(std::abs(wmHeight / wmsGeo[5]));

        const int CHUNK = 512;
        std::vector<GByte> dummyBuffer(CHUNK * CHUNK * 3);
        int bands[3] = {1, 2, 3};

        // 2. CALIBRATE RESOLUTION TO THE PADDED GEOMETRY
        double currentRes = wmWidth / (screenW * pad);
        const double MIN_RES = 1.0;

        int totalChunks = 0;
        for (double r = currentRes; r >= MIN_RES; r /= 2.0) {
            int targetXPixels = wmWidth / r;
            int targetYPixels = wmHeight / r;
            totalChunks += std::ceil(static_cast<double>(targetXPixels) / CHUNK) *
                           std::ceil(static_cast<double>(targetYPixels) / CHUNK);
        }
        if (totalChunks == 0) totalChunks = 1;

        int currentChunk = 0;
        int layer = 1;

        // 3. EXECUTE PYRAMID FETCH
        for (double r = currentRes; r >= MIN_RES; r /= 2.0) {
            int targetXPixels = wmWidth / r;
            int targetYPixels = wmHeight / r;

            for (int y = 0; y < targetYPixels; y += CHUNK) {
                for (int x = 0; x < targetXPixels; x += CHUNK) {
                    int readXSize = std::min(CHUNK, targetXPixels - x);
                    int readYSize = std::min(CHUNK, targetYPixels - y);

                    int srcXOff = xOff + (x * dsXSize / targetXPixels);
                    int srcYOff = yOff + (y * dsYSize / targetYPixels);
                    int srcXSize = readXSize * dsXSize / targetXPixels;
                    int srcYSize = readYSize * dsYSize / targetYPixels;

                    threadDS->RasterIO(GF_Read, srcXOff, srcYOff, srcXSize, srcYSize,
                                        dummyBuffer.data(), readXSize, readYSize,
                                        GDT_Byte, 3, bands, 3, readXSize * 3, 1);

                    QThread::msleep(50); // Paced to avoid IP ban

                    currentChunk++;
                    double pct = static_cast<double>(currentChunk) / totalChunks;
                    emit progressUpdated(QString("Caching Map Depth %1...").arg(layer), pct);
                }
            }
            layer++;
        }

        GDALClose(threadDS);
        emit progressUpdated("Offline Cache Complete", 1.0);
        emit downloadComplete();
    });
}

// void MapCanvasItem::paint(QPainter *painter) {
//     if (!m_dataset || m_viewWidthMeters <= 0 || m_viewHeightMeters <= 0 || m_utmWkt.isEmpty()) return;
//
//     int screenWidth = boundingRect().width();
//     int screenHeight = boundingRect().height();
//     if (screenWidth <= 0 || screenHeight <= 0) return;
//
//     // 1. Create an in-memory dataset matching the exact QML canvas dimensions
//     GDALDriver *memDriver = GetGDALDriverManager()->GetDriverByName("MEM");
//     if (!memDriver) return;
//
//     GDALDataset *memDS = memDriver->Create("", screenWidth, screenHeight, 3, GDT_Byte, nullptr);
//     if (!memDS) return;
//
//     // 2. Assign the UTM projection to the memory dataset
//     memDS->SetProjection(m_utmWkt.toStdString().c_str());
//
//     // 3. Map the dataset's pixels to the exact UTM meters of our current view
//     double mppX = m_viewWidthMeters / screenWidth;
//     double mppY = m_viewHeightMeters / screenHeight;
//     double viewMinX = m_utmCenterX - (m_viewWidthMeters / 2.0);
//     double viewMaxY = m_utmCenterY + (m_viewHeightMeters / 2.0);
//
//     // Set the GeoTransform (TopLeft X, X Pixel Size, X Rotation, TopLeft Y, Y Rotation, Y Pixel Size)
//     double geoTransform[6] = { viewMinX, mppX, 0.0, viewMaxY, 0.0, -mppY };
//     memDS->SetGeoTransform(geoTransform);
//
//     // 4. Perform a highly targeted reprojection directly into the memory dataset
//     const char* srcWkt = m_dataset->GetProjectionRef();
//     GDALReprojectImage(m_dataset, srcWkt,
//                        memDS, m_utmWkt.toStdString().c_str(),
//                        GRA_Bilinear, 0.0, 0.0, nullptr, nullptr, nullptr);
//
//     // 5. Read the crisp, reprojected pixels into the QImage buffer
//     std::vector<GByte> buffer(screenWidth * screenHeight * 4, 255);
//     int bands[3] = {1, 2, 3};
//
//     memDS->RasterIO(GF_Read, 0, 0, screenWidth, screenHeight,
//                     buffer.data(), screenWidth, screenHeight,
//                     GDT_Byte, 3, bands,
//                     4, screenWidth * 4, 1);
//
//     m_mapBuffer = QImage(buffer.data(), screenWidth, screenHeight, QImage::Format_RGBA8888).copy();
//     painter->drawImage(0, 0, m_mapBuffer);
//
//     // Clean up the memory dataset for this frame
//     GDALClose(memDS);
// }

void MapCanvasItem::drawEnvelope(const QVariantList& perimeterPoints) {
    m_rawEnvelope = perimeterPoints;
    update(); // Trigger paint() to overlay the polygon
}

void MapCanvasItem::paint(QPainter *painter) {
    if (!m_mapBuffer.isNull()) {
        // Fill the background with a neutral color so edges aren't jarring during a pan
        painter->fillRect(boundingRect(), QColor("#e8edf0"));

        // Draw the image offset by the current mouse drag distance
        painter->drawImage(m_visualPanX, m_visualPanY, m_mapBuffer);
    }

    // 2. Draw the HARP Envelope if data exists
    if (!m_rawEnvelope.isEmpty() && m_viewWidthMeters > 0) {
        int screenWidth = boundingRect().width();
        int screenHeight = boundingRect().height();

        // double mppX = m_viewWidthMeters / screenWidth;
        // double mppY = m_viewHeightMeters / screenHeight;
        // double viewMinX = m_utmCenterX - (m_viewWidthMeters / 2.0);
        // double viewMaxY = m_utmCenterY + (m_viewHeightMeters / 2.0);
        // Use the locked RENDER coordinates, not the live camera coordinates
        double mppX = m_renderViewWidth / screenWidth;
        double mppY = m_renderViewHeight / screenHeight;
        double viewMinX = m_renderUtmCenterX - (m_renderViewWidth / 2.0);
        double viewMaxY = m_renderUtmCenterY + (m_renderViewHeight / 2.0);

        // Setup projection transformers (WGS84 -> Current UTM Zone)
        OGRSpatialReference latLonSrs, utmSrs;
        latLonSrs.SetWellKnownGeogCS("WGS84");
        latLonSrs.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
        utmSrs.importFromWkt(m_utmWkt.toStdString().c_str());
        utmSrs.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);

        OGRCoordinateTransformation *coordTrans = OGRCreateCoordinateTransformation(&latLonSrs, &utmSrs);

        QPolygonF envelopePolygon;

        for (const QVariant& vPt : m_rawEnvelope) {
            QVariantMap pt = vPt.toMap();
            double lon = pt["lon"].toDouble();
            double lat = pt["lat"].toDouble();

            if (coordTrans) {
                coordTrans->Transform(1, &lon, &lat); // Overwrites with UTM X/Y
            }

            // Convert UTM spatial coordinates to Screen Pixels (incorporating active visual pan)
            double screenX = ((lon - viewMinX) / mppX) + m_visualPanX;
            double screenY = ((viewMaxY - lat) / mppY) + m_visualPanY;

            envelopePolygon.append(QPointF(screenX, screenY));
        }

        if (coordTrans) OCTDestroyCoordinateTransformation(coordTrans);

        // Render the polygon with transparency
        painter->setRenderHint(QPainter::Antialiasing);
        painter->setPen(QPen(QColor(255, 0, 0, 200), 2)); // Red border
        painter->setBrush(QColor(255, 0, 0, 50));         // Semi-transparent red fill
        painter->drawPolygon(envelopePolygon);
    }

    // 3. Draw Markers (LZ and DZ)
    if ((m_showLz || m_showDz) && m_renderViewWidth > 0) {
        int screenWidth = boundingRect().width();
        int screenHeight = boundingRect().height();
        double mppX = m_renderViewWidth / screenWidth;
        double mppY = m_renderViewHeight / screenHeight;
        double viewMinX = m_renderUtmCenterX - (m_renderViewWidth / 2.0);
        double viewMaxY = m_renderUtmCenterY + (m_renderViewHeight / 2.0);

        OGRSpatialReference latLonSrs, utmSrs;
        latLonSrs.SetWellKnownGeogCS("WGS84");
        latLonSrs.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
        utmSrs.importFromWkt(m_utmWkt.toStdString().c_str());
        utmSrs.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
        OGRCoordinateTransformation *coordTrans = OGRCreateCoordinateTransformation(&latLonSrs, &utmSrs);

        painter->setRenderHint(QPainter::Antialiasing);

        if (m_showDz && coordTrans) {
            double lon = m_dzLon; double lat = m_dzLat;
            coordTrans->Transform(1, &lon, &lat); // Transforms Lat/Lon to UTM
            double screenX = ((lon - viewMinX) / mppX) + m_visualPanX;
            double screenY = ((viewMaxY - lat) / mppY) + m_visualPanY;

            // Draw infinite flight line
            double rad = m_acHdg * M_PI / 180.0;
            double dx = std::sin(rad);
            double dy = -std::cos(rad);
            QPointF p1(screenX - dx * 3000, screenY - dy * 3000);
            QPointF p2(screenX + dx * 3000, screenY + dy * 3000);

            painter->setPen(QPen(QColor(0, 0, 0, 150), 2, Qt::DashLine));
            painter->drawLine(p1, p2);

            // Draw T-Point rotated to heading
            painter->save();
            painter->translate(screenX, screenY);
            painter->rotate(m_acHdg); // Rotates the painter coordinate system to Aircraft Heading
            painter->setPen(QPen(QColor(255, 140, 0), 3)); // Orange T
            painter->drawLine(-10, -10, 10, -10); // Crossbar (leads flight path)
            painter->drawLine(0, -10, 0, 10);     // Stem

            painter->setPen(QColor(0, 0, 0));
            painter->drawText(15, -15, "DZ");
            painter->restore();
        }

        if (m_showLz && coordTrans) {
            double lon = m_lzLon; double lat = m_lzLat;
            coordTrans->Transform(1, &lon, &lat);
            double screenX = ((lon - viewMinX) / mppX) + m_visualPanX;
            double screenY = ((viewMaxY - lat) / mppY) + m_visualPanY;

            painter->setPen(QPen(QColor(0, 100, 255), 2));
            painter->setBrush(QColor(0, 100, 255, 100));
            painter->drawEllipse(QPointF(screenX, screenY), 7, 7);

            painter->setPen(QColor(0, 0, 0));
            painter->drawText(screenX + 10, screenY + 5, "LZ");
        }

        if (coordTrans) OCTDestroyCoordinateTransformation(coordTrans);
    }

    // // ... [existing envelope drawing code inside paint()] ...
    //
    // // 3. Draw the Central Crosshairs
    // int screenW = boundingRect().width();
    // int screenH = boundingRect().height();
    //
    // painter->setPen(QPen(QColor(0, 0, 0, 100), 1, Qt::DashLine));
    // painter->drawLine(screenW / 2, 0, screenW / 2, screenH); // Y Axis
    // painter->drawLine(0, screenH / 2, screenW, screenH / 2); // X Axis
    //
    // painter->setPen(QPen(QColor(255, 0, 0, 200), 1.5));
    // painter->drawEllipse(QPointF(screenW / 2, screenH / 2), 6, 6); // Center Reticle
}

// QPointF MapCanvasItem::screenToGeo(int mouseX, int mouseY) {
//     if (m_viewWidthMeters <= 0 || m_viewHeightMeters <= 0 || m_utmWkt.isEmpty()) return QPointF(0, 0);
//
//     int sw = boundingRect().width();
//     int sh = boundingRect().height();
//     if (sw <= 0 || sh <= 0) return QPointF(0, 0);
//
//     double mppX = m_viewWidthMeters / sw;
//     double mppY = m_viewHeightMeters / sh;
//
//     double viewMinX = m_utmCenterX - (m_viewWidthMeters / 2.0);
//     double viewMaxY = m_utmCenterY + (m_viewHeightMeters / 2.0);
//
//     // Apply active drag visual offset
//     double utmX = viewMinX + ((mouseX - m_visualPanX) * mppX);
//     double utmY = viewMaxY - ((mouseY - m_visualPanY) * mppY);
//
//     OGRSpatialReference latLonSrs, utmSrs;
//     latLonSrs.SetWellKnownGeogCS("WGS84");
//     latLonSrs.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
//     utmSrs.importFromWkt(m_utmWkt.toStdString().c_str());
//     utmSrs.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
//
//     OGRCoordinateTransformation *invTrans = OGRCreateCoordinateTransformation(&utmSrs, &latLonSrs);
//     double lon = utmX;
//     double lat = utmY;
//     if (invTrans) {
//         invTrans->Transform(1, &lon, &lat);
//         OCTDestroyCoordinateTransformation(invTrans);
//     }
//     return QPointF(lat, lon); // QPointF.x = Lat, QPointF.y = Lon
// }
//
// QPointF MapCanvasItem::getCenterGeo() {
//     // Return the exact center by converting the raw UTM center to Lat/Lon
//     if (m_utmWkt.isEmpty()) return QPointF(0, 0);
//
//     OGRSpatialReference latLonSrs, utmSrs;
//     latLonSrs.SetWellKnownGeogCS("WGS84");
//     latLonSrs.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
//     utmSrs.importFromWkt(m_utmWkt.toStdString().c_str());
//     utmSrs.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
//
//     OGRCoordinateTransformation *invTrans = OGRCreateCoordinateTransformation(&utmSrs, &latLonSrs);
//     double lon = m_utmCenterX;
//     double lat = m_utmCenterY;
//     if (invTrans) {
//         invTrans->Transform(1, &lon, &lat);
//         OCTDestroyCoordinateTransformation(invTrans);
//     }
//     return QPointF(lat, lon);
// }
//
// double MapCanvasItem::getViewWidthKm() {
//     return m_viewWidthMeters / 1000.0;
// }
//
// void MapCanvasItem::setViewWidthKm(double km) {
//     if (km <= 0) return;
//     m_viewWidthMeters = km * 1000.0;
//     // Aspect ratio auto-corrects height in refreshMapBuffer
//     refreshMapBuffer();
//     emit viewChanged();
// }
// QPointF MapCanvasItem::screenToLatLon(int x, int y) {
//     if (m_viewWidthMeters <= 0 || m_viewHeightMeters <= 0 || m_utmWkt.isEmpty()) return QPointF(0, 0);
//     int sw = boundingRect().width();
//     int sh = boundingRect().height();
//     if (sw <= 0 || sh <= 0) return QPointF(0, 0);
//
//     double mppX = m_viewWidthMeters / sw;
//     double mppY = m_viewHeightMeters / sh;
//
//     // Account for active dragging offset
//     double mapX = x - m_visualPanX;
//     double mapY = y - m_visualPanY;
//
//     // Convert screen pixel to UTM meters
//     double utmX = (m_utmCenterX - m_viewWidthMeters / 2.0) + (mapX * mppX);
//     double utmY = (m_utmCenterY + m_viewHeightMeters / 2.0) - (mapY * mppY);
//
//     // Transform UTM back to WGS84
//     OGRSpatialReference latLonSrs, utmSrs;
//     latLonSrs.SetWellKnownGeogCS("WGS84");
//     latLonSrs.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
//     utmSrs.importFromWkt(m_utmWkt.toStdString().c_str());
//     utmSrs.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
//
//     OGRCoordinateTransformation *invTrans = OGRCreateCoordinateTransformation(&utmSrs, &latLonSrs);
//     if (invTrans) {
//         invTrans->Transform(1, &utmX, &utmY);
//         OCTDestroyCoordinateTransformation(invTrans);
//     }
//
//     // Return Lon (X) and Lat (Y)
//     return QPointF(utmX, utmY);
// }
QPointF MapCanvasItem::screenToLatLon(int x, int y) {
    if (m_viewWidthMeters <= 0 || m_viewHeightMeters <= 0 || m_utmWkt.isEmpty()) return QPointF(0, 0);
    int sw = boundingRect().width();
    int sh = boundingRect().height();
    if (sw <= 0 || sh <= 0) return QPointF(0, 0);

    double mppX = m_viewWidthMeters / sw;
    double mppY = m_viewHeightMeters / sh;

    double mapX = x - m_visualPanX;
    double mapY = y - m_visualPanY;

    double utmX = (m_utmCenterX - m_viewWidthMeters / 2.0) + (mapX * mppX);
    double utmY = (m_utmCenterY + m_viewHeightMeters / 2.0) - (mapY * mppY);

    OGRSpatialReference latLonSrs, utmSrs;
    latLonSrs.SetWellKnownGeogCS("WGS84");
    latLonSrs.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
    utmSrs.importFromWkt(m_utmWkt.toStdString().c_str());
    utmSrs.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);

    OGRCoordinateTransformation *invTrans = OGRCreateCoordinateTransformation(&utmSrs, &latLonSrs);
    if (invTrans) {
        invTrans->Transform(1, &utmX, &utmY);
        OCTDestroyCoordinateTransformation(invTrans);
        return QPointF(utmX, utmY);
    }

    // [THE FIX]: Safely return 0,0 instead of passing unprojected millions of meters to the UI
    return QPointF(0, 0);
}

QPointF MapCanvasItem::getCenterLatLon() {
    int sw = boundingRect().width();
    int sh = boundingRect().height();
    // Re-use the screen converter to get the exact true center of the canvas
    return screenToLatLon(sw / 2, sh / 2);
}

double MapCanvasItem::getViewWidthKm() {
    return m_viewWidthMeters / 1000.0;
}

void MapCanvasItem::setLzMarker(double lat, double lon) {
    m_lzLat = lat; m_lzLon = lon; m_showLz = true; update();
}
void MapCanvasItem::setDzMarker(double lat, double lon, double heading) {
    m_dzLat = lat; m_dzLon = lon; m_acHdg = heading; m_showDz = true; update();
}
void MapCanvasItem::clearDzMarker() {
    m_showDz = false; update();
}
void MapCanvasItem::updateFlightLineHeading(double heading) {
    m_acHdg = heading;
    if (m_showDz) update();
}

MapCanvasItem::~MapCanvasItem() {
    if (m_dataset) {
        GDALClose(m_dataset);
    }
    VSIUnlink("/vsimem/osm_config.xml");
}