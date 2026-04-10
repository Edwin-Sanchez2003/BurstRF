#ifndef IMAGEBACKEND_H
#define IMAGEBACKEND_H

#include <QObject>
#include <QImage>
#include <vector>
#include <complex>
#include <cmath>

/*
 *
 * ⚠️ Thread safety: generateChunk will be called from worker threads. If it accesses shared state, you need mutexes.
 *
 */

class ImageBackend : public QObject {
    Q_OBJECT
public:
    explicit ImageBackend(QObject* parent = nullptr) : QObject(parent) {}

    // Called from background thread — must be thread-safe!
    // Returns a chunk of height `chunkH` starting at pixel row `yOffset`
    QImage generateChunk(int yOffset, int chunkW, int chunkH) {
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

    /*
     * TODO: Write new generateChunk Function, that takes the chunkW and chunkH & builds
     * a fake signal to display within that chunk.
     */


    /*
     * Generates a complex chirp signal that sweeps around a center frequency.
     * centerFreq: center frequency as a fraction of sample rate (0.0 - 0.5).
     * bandwidth: how wide the sweep is (fraction of sample rate).
     * sampleRate: samples per second.
     * duration: length of signal in seconds.
     */
    std::vector<std::complex<float>> generateChirp(float centerFreq, float bandwidth, float sampleRate, float duration)
    {
        int N = static_cast<int>(sampleRate * duration);
        std::vector<std::complex<flaot>> signal(N);
        float startFreq = centerFreq - (bandwidth / 2.0f);
        float stopFreq = centerFreq + (bandwidth / 2.0f);
        float chirpRate = (stopFreq - startFreq) / duration;

        for (int n = 0; n < N; n++)
        {
            float t = n / sampleRate;
            float phase = 2.0f * M_PI * (startFreq * t * 0.5f * chirpRate * t * t);
            signal[n] = std::polar(1.0f, phase);
        }

        return signal;
    }
};

#endif // IMAGEBACKEND_H
