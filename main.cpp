#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include "ImageBackend.h"
#include "ChunkImageProvider.h"

#include <iostream>
#include <sigmf_io/recording.h>


int main(int argc, char *argv[])
{

    sigmf_io::Recording recording = sigmf_io::Recording("/var/home/edwsanch/Downloads/trimmedSamples.sigmf-meta");
    std::cout << recording.data_path() << "\n";

    QGuiApplication app(argc, argv);
    QQmlApplicationEngine engine;

    ImageBackend* backend = new ImageBackend(&app);
    ChunkImageProvider* imgProvider = new ChunkImageProvider(backend);

    // Register Image Provider - engine takes ownership of the provider
    engine.addImageProvider("chunks", imgProvider);

    // Expose backend to QML if needed for other calls
    engine.rootContext()->setContextProperty("imageBackend", backend);

    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreationFailed,
        &app,
        []() { QCoreApplication::exit(-1); },
        Qt::QueuedConnection);
    engine.loadFromModule("BurstRF", "Main");

    return app.exec();
}
