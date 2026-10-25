#ifndef BURSTRF_BACKEND_H
#define BURSTRF_BACKEND_H

#include <QObject>
#include <QImage>
#include <QMutex>
#include <QMutexLocker>
#include <QUrl>
#include <QPainter>
#include <vector>
#include <complex>
#include <cmath>

#include <sigmf_io/recording.h>
#include "spectrogram_builder.h"

class BurstRFBackend : public QObject {
    Q_OBJECT
public:
    explicit BurstRFBackend(QObject* parent = nullptr) : QObject(parent) {}

    // Loads a SigMF Metadata file, given a QUrl.
    Q_INVOKABLE bool loadFile(const QUrl &url) {
        auto rec = std::make_unique<sigmf_io::Recording>(url.toLocalFile().toStdString());

        // TODO: need to have channels be dynamic - hardcoded to 1 for now...
        m_recording = std::move(rec);
        emit recordingLoaded();
        return true;
    }

    // Called by QML at startup and after a file is picked.
    Q_INVOKABLE void refreshChunk()
    {
        QImage img = m_recording ? generateChunk()
                                 : generatePlaceholder(1024, 512);
        emit chunkReady(img);
    }

    // Called from background thread — must be thread-safe!
    QImage generateChunk() {
        int64_t chunk_width = 1024;
        int64_t chunk_height = 512;
        if (!m_recording) return generatePlaceholder(chunk_width, chunk_height);

        int64_t channel = 1;
        int64_t sample_start = 0;
        int64_t sample_count = 200'000;
        int64_t available = m_recording->size(channel) - sample_start;
        if (available <= 0) return generatePlaceholder(chunk_width, chunk_height); // past EOF
        int64_t toRead = std::min(sample_count, available);

        std::vector<std::complex<double>> samples = m_recording->get_samples<std::complex<double>>(sample_start, toRead, channel);
        QImage img = this->spectrogram_builder.generateSpectrogram(samples);

        return img;
    }

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
    void chunkReady(const QImage &img);

private:
    std::unique_ptr<sigmf_io::Recording> m_recording;
    burstrf::SpectrogramBuilder spectrogram_builder;
};

#endif // BURSTRF_BACKEND_H
