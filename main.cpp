#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include "ImageBackend.h"
#include "ChunkImageProvider.h"
#include "SpectrogramBuilder.h"

void test_spectrogram();

int main(int argc, char *argv[])
{

    test_spectrogram();
    return 0;

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


void test_spectrogram()
{
    SpectrogramBuilder spec_builder;

    sigmf_io::Recording recording = sigmf_io::Recording("/var/home/edwsanch/Downloads/trimmedSamples(2).sigmf-meta");
    std::vector<std::complex<double>> samples = recording.get_samples<std::complex<double>>(0, 1000000);

    QImage spectrogram = spec_builder.generateSpectrogram(samples);
    spec_builder.saveAsPNG(spectrogram, QString("/var/home/edwsanch/Downloads/trimmedSamples(2)__0__1_000_000.png"));
}
