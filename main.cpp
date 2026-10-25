#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
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

    sigmf_io::Recording recording = sigmf_io::Recording("trimmedSamples(2).sigmf-meta");
    std::vector<std::complex<double>> samples = recording.get_samples<std::complex<double>>(0, 1000000);

    QImage spectrogram = spec_builder.generateSpectrogram(samples);
    spec_builder.saveAsPNG(spectrogram, QString("trimmedSamples(2)_auto_0__1_000_000.png"));
}
