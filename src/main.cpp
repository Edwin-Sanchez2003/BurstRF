#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <memory>

#include "./include/burstrf_backend.h"

int main(int argc, char *argv[])
{

    QGuiApplication app(argc, argv);
    QQmlApplicationEngine engine;

    // Backend is the interface between the UI and the core logic.
    BurstRFBackend* backend = new BurstRFBackend(&app);

    // Expose backend to QML if needed for other calls
    engine.rootContext()->setContextProperty("backend", backend);

    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreationFailed,
        &app,
        []() { QCoreApplication::exit(-1); },
        Qt::QueuedConnection);
    engine.loadFromModule("BurstRF", "Main");

    return app.exec();
}
