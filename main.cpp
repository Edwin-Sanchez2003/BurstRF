#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <memory>

#include "burstrf_backend.h"
#include "spectrogram_builder.h"

void test_spectrogram();

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


void test_spectrogram()
{
    burstrf::SpectrogramBuilder spec_builder;

    std::shared_ptr<sigmf_io::Recording> recording = std::make_shared<sigmf_io::Recording>("trimmedSamples(2).sigmf-meta");
    spec_builder.setRecording(recording);

    QImage spectrogram = spec_builder.generateSpectrogramChunk(0);
    spec_builder.saveAsPNG(spectrogram, QString("trimmedSamples(2)_auto_0__1_000_000.png"));
}
