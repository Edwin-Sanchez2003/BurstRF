#ifndef BURSTRF_BACKEND_H
#define BURSTRF_BACKEND_H

#include <QObject>
#include <QImage>
#include <QMutex>
#include <QMutexLocker>
#include <QUrl>
#include <QPainter>
#include <QDebug>
#include <vector>
#include <complex>
#include <cmath>

#include <sigmf_io/recording.h>
#include "spectrogram_builder.h"

// TODO: Move implementations to .cpp file...
/*
 * * Migrate stateful values to Q_PROPERTIES to better sync frontend display.
 * * With this, we can establish an invariant for "no file" where these properties
 *   have default values, reducing chance for invalid state - default chunk_size, chunk_count, width, etc. etc.
 * * Do propper clearing of state when a file is changed/switched - clear out UI, clear out recording, etc. etc.
 *
 */

class BurstRFBackend : public QObject {
    Q_OBJECT
public:
    explicit BurstRFBackend(QObject* parent = nullptr) : QObject(parent) {}

    // Loads a SigMF Metadata file, given a QUrl.
    Q_INVOKABLE bool loadFile(const QUrl &url) {
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

    // Width of the spectrogram... Remove?
    int64_t spectrogramWidth() const { return this->spectrogram_builder.width(); }

    // Used by QML frontend to get the height of a single chunk of the spectrogram for chunk display.
    // TODO: Make this a Q_PROPERTY -> that way it can automatically update/adjust when the chunk height changes.
    Q_INVOKABLE int64_t spectrogramChunkHeight() const {
        int64_t chunk_width = this->spectrogramWidth();
        return this->chunk_size / chunk_width + (this->chunk_size % chunk_width != 0 ? 1 : 0);
    }

    // Used by QML frontend to know how many spectrogram chunk objects to put in the scrollable window (flickable).
    // TODO: make this a Q_PROPERTY as well...
    Q_INVOKABLE int64_t spectrogramChunkCount() const
    {
        if (!m_recording) return 3;

        int64_t rec_size = this->m_recording->size(1);
        int64_t chunkCount = rec_size / this->chunk_size + (rec_size % this->chunk_size != 0 ? 1 : 0);
        return chunkCount;
    }

    // Called by QML to get a specific chunk generated from the spectrogram
    // Call from background thread — must be thread-safe! (Recording is the only worry-point here).
    // TODO: change the frontend binding to be a signal w/chunk_index, so this can run on a separate thread from the display thread!
    Q_INVOKABLE void generateChunk(int64_t chunk_index) {
        // If no recording loaded, generate a placeholder the size of the expected chunk.
        int64_t chunk_width = this->spectrogramWidth();
        int64_t chunk_height = this->chunk_size / chunk_width + (this->chunk_size % chunk_width != 0 ? 1 : 0);

        // If no recording, return placeholder
        if (!m_recording)
        {
            emit chunkReady(chunk_index, generatePlaceholder(this->spectrogramWidth(), chunk_height));
            return;
        }

        // convert the chunk_index to a sample_start, based on the chunk_size
        int64_t sample_start = this->chunk_size * chunk_index;

        int64_t channel = 1;
        int64_t available = m_recording->size(channel) - sample_start;
        if (available <= 0) {
            emit chunkReady(chunk_index, generatePlaceholder(chunk_width, chunk_height)); // past EOF
            return;
        }

        int64_t toRead = std::min(this->chunk_size, available);

        std::vector<std::complex<double>> samples = m_recording->get_samples<std::complex<double>>(sample_start, toRead, channel);
        QImage img = this->spectrogram_builder.generateSpectrogram(samples);

        emit chunkReady(chunk_index, img);
    }


    // TODO: couple with spectrogram builder so we can have a consistent size with a current recording when waiting for a
    // chunk to load. Should still allow for it to be called w/out a Recording object, as we need to set placeholders for "no file"
    // scenario.
    QImage generatePlaceholder(int chunkW, int chunkH)
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

signals:
    void recordingLoaded();
    void chunkReady(int64_t index, const QImage &img);

private:
    int64_t chunk_size = 600'000;
    std::unique_ptr<sigmf_io::Recording> m_recording;
    burstrf::SpectrogramBuilder spectrogram_builder;
};

#endif // BURSTRF_BACKEND_H
