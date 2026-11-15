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

#include <sigmf_io/recording.h>
#include "spectrogram_builder.h"
#include "tools.h"


BurstRFBackend::BurstRFBackend(QObject* parent)
    : QObject(parent)
{
    qRegisterMetaType<int64_t>("int64_t");   // lets queued connections carry int64_t
    m_worker.moveToThread(&m_thread);

    // Runs on the worker thread. Qt queues each request to it from the GUI thread.
    // TODO: make a queue for chunks, so that when a user scrolls far beyond the currently viewed
    // chunk, the requested chunk is canceled (otherwise a quick scroll make a lot of chunks to display
    // but the user doesn't actually see them as they've already scrolled past...)
    connect(this, &BurstRFBackend::requestChunk, &m_worker, [this](int64_t chunk_index) {
        //QElapsedTimer timer;
        //timer.start();

        QImage img;
        try {
            img = buildChunk(chunk_index);
        } catch (const std::exception &e) {
            qWarning() << "Chunk" << chunk_index << "failed:" << e.what();
            img = generatePlaceholder(spectrogramWidth(), spectrogramChunkHeight());
        }

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
    if (m_pending.loadAcquire() > 0) {
        qWarning() << "Cannot load a file while spectrogram chunks are being generated.";
        return false;
    }

    auto rec = std::make_unique<sigmf_io::Recording>(url.toLocalFile().toStdString());

    // TODO: need to have channels be dynamic - hardcoded to 1 for now...
    m_recording = std::move(rec);

    // TEMP: immediately calculate db scale at initial load
    int64_t channel = 1;
    int64_t available = m_recording->size(channel);
    int64_t toRead = std::min(this->chunk_size, available);
    std::vector<std::complex<double>> samples = m_recording->get_samples<std::complex<double>>(0, toRead, channel);
    spectrogram_builder.computeSetDbScale(samples);
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
double BurstRFBackend::getCenterFrequencyHz() const
{
    if(!m_recording) return 0.0;
    std::optional<double> center_freq = m_recording->meta.capture_at(0).frequency();
    if(center_freq.has_value())
        return center_freq.value();
    return 0.0;
}

// functions to get needed information for the Frequency Axis
double BurstRFBackend::getSampleRateSps() const
{
    if(!m_recording) return 1.0;
    std::optional<double> sample_rate = m_recording->meta.global.sample_rate();
    if(sample_rate.has_value())
        return sample_rate.value();
    return 1.0;
}


int64_t BurstRFBackend::spectrogramChunkHeight() const
{
    return burstrf::ceilDiv<int64_t>(this->chunk_size, this->spectrogramWidth());
}


// Used by QML frontend to know how many spectrogram chunk objects to put in the scrollable window (flickable).
// TODO: handle channels & invariants properly!
int64_t BurstRFBackend::spectrogramChunkCount() const
{
    if (!m_recording) return 3;
    return burstrf::ceilDiv<int64_t>(this->m_recording->size(1), this->chunk_size);
}


// Used by QML to get the total number of rows for the spectrogram view.
// TODO: handle channels & invariants properly!!!
// NOTE: when spectrogram uses noverlap, this logic will need to be updated...
int64_t BurstRFBackend::totalRows() const
{
    // If no recording loaded, use 3 placeholders' worth of expected rows.
    if (!m_recording)
        return 3 * spectrogramChunkHeight();

    return burstrf::ceilDiv<int64_t>(this->m_recording->size(1), this->spectrogramWidth());
}


// Called by QML. Queues the work onto the worker thread and returns immediately.
void BurstRFBackend::generateChunk(int64_t chunk_index)
{
    m_pending.ref();
    emit requestChunk(chunk_index);
}


// Builds a placeholder chunk for when there's no recording.
QImage BurstRFBackend::generatePlaceholder(int chunkW, int chunkH) const
{
    QImage img(chunkW, chunkH, QImage::Format_RGB32);

    const int border  = 6;
    const int gridSz  = 32;  // checkerboard cell size — equal W & H means any stretch is obvious

    for (int row = 0; row < chunkH; ++row) {
        auto* line = reinterpret_cast<QRgb*>(img.scanLine(row));
        bool onVBorder = (row < border || row >= chunkH - border);

        for (int col = 0; col < chunkW; ++col) {
            bool onHBorder = (col < border || col >= chunkW - border);

            if (onVBorder || onHBorder) {
                // Red border — thickness is equal on all sides, so squash/stretch is visible
                line[col] = qRgb(255, 0, 0);
            } else {
                // Checkerboard interior: cells are square, so any non-uniform scale is obvious
                bool checker = ((row / gridSz) + (col / gridSz)) % 2 == 0;
                line[col] = checker ? qRgb(220, 220, 220) : qRgb(40, 40, 40);
            }
        }
    }

    // Crosshair through the exact centre — a circle would be ideal but costly;
    // two 1-px lines are cheap and make translation/offset errors visible too.
    const int cx = chunkW / 2;
    const int cy = chunkH / 2;
    const int chLen = 20;  // half-length of each arm

    for (int r = cy - chLen; r <= cy + chLen; ++r)
        if (r >= 0 && r < chunkH)
            reinterpret_cast<QRgb*>(img.scanLine(r))[cx] = qRgb(0, 255, 0);

    auto* midLine = reinterpret_cast<QRgb*>(img.scanLine(cy));
    for (int c = cx - chLen; c <= cx + chLen; ++c)
        if (c >= 0 && c < chunkW)
            midLine[c] = qRgb(0, 255, 0);

    return img;
}


// Runs on the worker thread. Returns the chunk image (or a placeholder).
QImage BurstRFBackend::buildChunk(int64_t chunk_index) const
{
    // If no recording, generate a placeholder the size of the expected chunk.
    if (!m_recording)
        return generatePlaceholder(this->spectrogramWidth(), spectrogramChunkHeight());

    // convert the chunk_index to a sample_start, based on the chunk_size
    int64_t sample_start = this->chunk_size * chunk_index;

    int64_t channel = 1;
    int64_t available = m_recording->size(channel) - sample_start;
    if (available <= 0)
        return generatePlaceholder(this->spectrogramWidth(), spectrogramChunkHeight()); // past EOF

    int64_t toRead = std::min(this->chunk_size, available);

    std::vector<std::complex<double>> samples = m_recording->get_samples<std::complex<double>>(sample_start, toRead, channel);
    return this->spectrogram_builder.generateSpectrogram(samples);
}
