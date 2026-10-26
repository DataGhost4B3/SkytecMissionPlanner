// #include <QGuiApplication>
#include <QApplication>
#include <QQmlApplicationEngine>
#include <QCoreApplication>
#include <QDirIterator>
#include <QDebug>
#include <QtGlobal> // Required for OS macros

int main(int argc, char *argv[]) {
#ifdef Q_OS_LINUX
    // qputenv("QT_QPA_PLATFORMTHEME", "xdgdesktopportal");
    // qputenv("XDG_CURRENT_DESKTOP", "KDE");
    // qputenv("QT_QPA_PLATFORMTHEME", "kde");
    qputenv("QT_QPA_PLATFORMTHEME", "gtk3");
#endif

    QApplication app(argc, argv);

    QCoreApplication::setOrganizationName("Skytec Solutions Pvt. Ltd.");
    QCoreApplication::setApplicationName("MP");

    // Place this in your main.cpp
    QDirIterator it(":", QDirIterator::Subdirectories);
    while (it.hasNext()) {
        qDebug() << it.next();
    }

    QQmlApplicationEngine engine;

    // Tell the engine where QML modules are located in the resource system
    engine.addImportPath(QStringLiteral("qrc:/qt/qml"));

    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreationFailed,
        &app,
        []() {
            QCoreApplication::exit(-1);
        },
        Qt::QueuedConnection
        );

    const QUrl url(QStringLiteral("qrc:/qt/qml/ui/Main.qml"));
    engine.load(url);

    return app.exec();
}