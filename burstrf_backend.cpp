#include "burstrf_backend.h"

#include <QObject>
#include <QImage>
#include <QMutex>
#include <QMutexLocker>
#include <QUrl>
#include <QPainter>
#include <QDebug>
#include <QThread>
#include <QAtomicInt>
#include <QElapsedTimer>
#include <QString>
#include <QLocale>
#include <vector>
#include <complex>
#include <cmath>
#include <optional>
#include <memory>

#include <sigmf_io/recording.h>
#include "spectrogram_builder.h"
#include "tools.h"


BurstRFBackend::BurstRFBackend(QObject* parent)
    : QObject(parent)
{
    qRegisterMetaType<size_t>("size_t");   // lets queued connections carry size_t
    qRegisterMetaType<int64_t>("int64_t");   // lets queued connections carry size_t
    m_worker.moveToThread(&m_thread);

    // Runs on the worker thread. Qt queues each request to it from the GUI thread.
    // TODO: make a queue for chunks, so that when a user scrolls far beyond the currently viewed
    // chunk, the requested chunk is canceled (otherwise a quick scroll make a lot of chunks to display
    // but the user doesn't actually see them as they've already scrolled past...)
    connect(this, &BurstRFBackend::requestChunk, &m_worker, [this](int64_t chunk_index) {
        //QElapsedTimer timer;
        //timer.start();
        QImage img = this->spectrogram_builder.generateSpectrogramChunk(chunk_index);

        //qDebug() << "Chunk" << chunk_index << "finished in" << timer.elapsed() << "ms"
        //         << "| pending after:" << m_pending.loadAcquire() - 1;

        emit chunkReady(chunk_index, img);
        m_pending.deref();   // always runs, even after an exception
    });

    m_thread.start();
}


BurstRFBackend::~BurstRFBackend()
{
    m_thread.quit();
    m_thread.wait();   // lets any in-progress chunk finish before members are destroyed
}


// Loads a SigMF Metadata file, given a QUrl.
bool BurstRFBackend::loadFile(const QUrl &url)
{
    // protects threads from pointer changes during execution.
    if (m_pending.loadAcquire() > 0) {
        qWarning() << "Cannot load a file while spectrogram chunks are being generated.";
        return false;
    }

    // set recording pointer.
    this->m_recording = std::make_shared<sigmf_io::Recording>(url.toLocalFile().toStdString());

    // set spectrogrambuilder.
    this->spectrogram_builder.setRecording(this->m_recording);

    // immediately calculate db scale at initial load
    this->spectrogram_builder.computeSetDbScale();

    emit recordingLoaded();
    return true;
}


// QML-callable function to get the current recording's file anme.
QString BurstRFBackend::currentFileDescriptor() const
{
    // empty string if there's no recording.
    if (!m_recording) return QString::fromStdString("");

    // build file descriptor using file path, channel, size, and chunk count
    std::string file_descriptor = this->m_recording->meta_path();
    file_descriptor += " | Channel: 1";
    file_descriptor += " | Samples: " + QLocale("en_US").toString(this->m_recording->size(1)).toStdString();
    file_descriptor += " | Chunk Count: " + std::to_string(this->spectrogramChunkCount());
    return QString::fromStdString(file_descriptor);
}


// functions to get needed information for the Frequency Axis
// NOTE: move to SpectrogramBuilder? prob not, wait for now...
double BurstRFBackend::getCenterFrequencyHz() const
{
    if(!m_recording) return 0.0;
    std::optional<double> center_freq = m_recording->meta.capture_at(0).frequency();
    if(center_freq.has_value())
        return center_freq.value();
    return 0.0;
}

// functions to get needed information for the Frequency Axis
// NOTE: move to spectrogramBuilder? prob not...
double BurstRFBackend::getSampleRateSps() const
{
    if(!m_recording) return 1.0;
    std::optional<double> sample_rate = m_recording->meta.global.sample_rate();
    if(sample_rate.has_value())
        return sample_rate.value();
    return 1.0;
}


// NOTE: move chunk_size & height to SpectrogramBuilder?
size_t BurstRFBackend::spectrogramChunkHeight() const
{
    return this->spectrogram_builder.params().spectrogram_chunk_height();
}


// Used by QML frontend to know how many spectrogram chunk objects to put in the scrollable window (flickable).
// TODO: handle channels & invariants properly!
// NOTE: should be on SpectrogramBuilder class?
size_t BurstRFBackend::spectrogramChunkCount() const
{
    return this->spectrogram_builder.chunkCount();
}


// Used by QML to get the total number of rows for the spectrogram view.
// TODO: handle channels & invariants properly!!!
// NOTE: when spectrogram uses noverlap, this logic will need to be updated...
// NOTE: should be on spectrogram builder class?
size_t BurstRFBackend::spectrogramRowCount() const
{
    return this->spectrogram_builder.rowCount();
}


// Called by QML. Queues the work onto the worker thread and returns immediately.
void BurstRFBackend::generateChunk(int64_t chunk_index)
{
    m_pending.ref();
    emit requestChunk(chunk_index);
}
