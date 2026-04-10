#ifndef IMAGEBACKEND_H
#define IMAGEBACKEND_H

#include <QObject>
#include <QImage>
#include <vector>
#include <complex>
#include <cmath>

#include <kiss_fft.h>

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
        //return generatePlaceholder(chunkW, chunkH);
        float sampleRate =  40000000.0f;
        float centerFreq = 300000000.0f;

        std::vector<std::complex<float>> signal = generateFakeSignal(chunkW, chunkH, sampleRate, centerFreq);
        return generateSpectrogram(signal, sampleRate, centerFreq, chunkW, chunkH);
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

    /*
     * TODO: Write new generateChunk Function, that takes the chunkW and chunkH, builds
     * a fake signal, and generates a spectrogram to display within that chunk.
     */
    std::vector<std::complex<float>> generateFakeSignal(int chunkW, int chunkH, float sampleRate, float centerFreq)
    {
        int fftSize = chunkH * 2; // bins = chunkH, so fftSize = chunkH * 2
        int hopSize = fftSize / 2;

        // We need exactly chunkW frames, each hopSize samples apart
        int numSamples = hopSize * chunkW + fftSize;
        float duration = numSamples / sampleRate;

        // Generate a chirp centered at centerFreq
        // Express centerFreq as a fraction of sampleRate for generateChirp
        float normalizedCenter = centerFreq / sampleRate;
        float bandwidth = normalizedCenter * 0.5f; // sweep +-25% of centerFreq

        return generateChirp(normalizedCenter, bandwidth, sampleRate, duration);

        QImage img(chunkW, chunkH, QImage::Format_RGB32);
    }

    QImage generateSpectrogram(const std::vector<std::complex<float>>& signal, float sampleRate, float centerFreq, int chunkW, int chunkH)
    {
        int fftSize = chunkH * 2;
        int hopSize = fftSize / 2;

        auto applyHann = [&](std::vector<kiss_fft_cpx>& buf, int N)
        {
            for (int i = 0; i < N; i++)
            {
                float w = 0.5f * (1.0f - std::cos(2.0f * M_PI * i / (N - 1)));
                buf[i].r *= w;
                buf[i].i *= w;
            }
        };

        kiss_fft_cfg cfg = kiss_fft_alloc(fftSize, false, nullptr, nullptr);

        float dbMin = -80.f, dbMax = 0.0f;
        std::vector<std::vector<float>> dbFrames;
        dbFrames.reserve(chunkW);

        int numCols = std::min(chunkW, (int)(signal.size() - fftSize) / hopSize + 1);

        for (int col = 0; col < numCols; col++)
        {
            int start = col * hopSize;

            std::vector<kiss_fft_cpx> in(fftSize), out(fftSize);
            for(int i = 0; i < fftSize; i++)
            {
                in[i].r = signal[start + i].real();
                in[i].i = signal[start + i].imag();
            }

            applyHann(in, fftSize);
            kiss_fft(cfg, in.data(), out.data());

            std::vector<float> db(chunkH);
            for (int i = 0; i < chunkH; i++)
            {
                float mag = std::sqrt(out[i].r * out[i].r + out[i].i * out[i].i);
                float val = 20.0f * std::log10(std::max(mag, 1e-6f));
                db[i] = (std::clamp(val, dbMin, dbMax) - dbMin) / (dbMax - dbMin);
            }
            dbFrames.push_back(db);
        }

        kiss_fft_free(cfg);

        QImage image(chunkW, chunkH, QImage::Format_RGB32);
        image.fill(Qt::black); // fill in case signal was shorter than chunkW.

        for(int x = 0; x < (int)dbFrames.size(); x++)
        {
            for (int y = 0; y < chunkH; y++)
            {
                float value = dbFrames[x][chunkH - 1 - y];
                QColor color = QColor::fromHsvF((1.0f - value) * 0.66f, 1.0f, value > 0.01f ? 1.0f : 0.0f);
                image.setPixel(x, y, color.rgb());
            }
        }

        return image;
    }

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
        std::vector<std::complex<float>> signal(N);
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
