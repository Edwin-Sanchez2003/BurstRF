#ifndef IMAGEBACKEND_H
#define IMAGEBACKEND_H

#include <QObject>
#include <QImage>
#include <QMutex>
#include <QMutexLocker>
#include <QUrl>
#include <QPainter>
#include <vector>
#include <complex>
#include <cmath>

#include <kiss_fft.h>
#include <sigmf_io/recording.h>

/*
 *
 * ⚠️ Thread safety: generateChunk will be called from worker threads. If it accesses shared state, you need mutexes.
 *
 */

class ImageBackend : public QObject {
    Q_OBJECT
    Q_PROPERTY(qint64 totalHeight READ totalHeight NOTIFY recordingLoaded)
    Q_PROPERTY(double sampleRate READ sampleRate NOTIFY recordingLoaded)
public:
    explicit ImageBackend(QObject* parent = nullptr) : QObject(parent) {}

    // Loads a SigMF Metadata file, given a QUrl.
    Q_INVOKABLE bool loadFile(const QUrl &url) {
        QMutexLocker lock(&m_mutex);
        auto rec = std::make_unique<sigmf_io::Recording>(url.toLocalFile().toStdString());

        // TODO: need to have channels be dynamic - hardcoded to 1 for now...
        m_recording = std::move(rec);
        m_sampleRate = m_recording->meta.global.sample_rate().value_or(-1.0);
        m_totalSamples = m_recording->data.size(m_recording->meta.captures(), 1);
        emit recordingLoaded();
        return true;
    }

    qint64 totalHeight() const {
        if (m_totalSamples < m_fftSize) return 0;
        return (m_totalSamples - m_fftSize) / m_hopSize + 1;
    }
    double sampleRate() const { return m_sampleRate; }

    // Called from background thread — must be thread-safe!
    QImage generateChunk(int yOffset, int chunkW, int chunkH) {
        QMutexLocker lock(&m_mutex);           // Recording access must be locked
        if (!m_recording) return generatePlaceholder(chunkW, chunkH);

        int64_t hop = m_hopSize, fft = m_fftSize;
        int64_t sample_start = static_cast<int64_t>(yOffset) * hop;
        int64_t sample_count = static_cast<int64_t>(chunkH - 1) * hop + fft;

        int64_t available = m_totalSamples - sample_start;
        if (available <= 0) return generatePlaceholder(chunkW, chunkH); // past EOF
        int64_t toRead = std::min(sample_count, available);

        auto signal = m_recording->get_samples<std::complex<float>>(sample_start, toRead, /*channel=*/1);
        QImage img = generateSpectrogram(signal, chunkW, chunkH);

        if (toRead < sample_count) {
            // pad the short tail with black rows so the image is still chunkH tall
            padBottomWithBlack(img, chunkH - (toRead - fft) / hop - 1);
        }
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
        float duration = 5.0f * numSamples / sampleRate;

        // Generate a chirp centered at centerFreq
        // Express centerFreq as a fraction of sampleRate for generateChirp
        float bandwidth = 200e3;  // instead of 2e6
        return generateChirp(centerFreq, bandwidth, sampleRate, duration);
    }

    QImage generateSpectrogram(
        const std::vector<std::complex<float>>& signal,
        int chunkW,
        int chunkH
    ) {
        int fftSize = chunkH * 2;
        int hopSize = fftSize / 2;
        int half = fftSize / 2;

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

        float dbMin = -60.f, dbMax = 0.0f;
        std::vector<std::vector<float>> dbFrames;
        dbFrames.reserve(chunkW);

        int numCols = std::min(chunkW, (int)(signal.size() - fftSize) / hopSize + 1);

        // 🔥 Centered crop around DC
        int startBin = half - (chunkH / 2);

        for (int col = 0; col < numCols; col++)
        {
            int start = col * hopSize;

            std::vector<kiss_fft_cpx> in(fftSize), out(fftSize);

            for (int i = 0; i < fftSize; i++)
            {
                in[i].r = signal[start + i].real();
                in[i].i = signal[start + i].imag();
            }

            applyHann(in, fftSize);
            kiss_fft(cfg, in.data(), out.data());

            std::vector<float> db(chunkH);

            for (int i = 0; i < chunkH; i++)
            {
                int idx = (startBin + i + fftSize) % fftSize;  // safe wrap
                int shifted = (idx + half) % fftSize;          // FFT shift

                float mag = std::sqrt(out[shifted].r * out[shifted].r +
                                      out[shifted].i * out[shifted].i) / fftSize;

                float val = 20.0f * std::log10(std::max(mag, 1e-6f));
                db[i] = (std::clamp(val, dbMin, dbMax) - dbMin) / (dbMax - dbMin);
            }

            dbFrames.push_back(db);
        }

        kiss_fft_free(cfg);

        // freq = X, time = Y
        QImage image(chunkH, chunkW, QImage::Format_RGB32);
        image.fill(Qt::black);

        for (int t = 0; t < (int)dbFrames.size(); t++)
        {
            for (int f = 0; f < chunkH; f++)
            {
                float value = dbFrames[t][chunkH - 1 - f];

                QColor color = QColor::fromHsvF(
                    (1.0f - value) * 0.66f,
                    1.0f,
                    value
                    );

                image.setPixel(f, t, color.rgb());
            }
        }

        return image;
    }

    /*
     * centerFreq: Hz
     * bandwidth: Hz
     * sampleRate: Hz
     */
    std::vector<std::complex<float>> generateChirp(float centerFreq, float bandwidth, float sampleRate, float duration)
    {
        int N = static_cast<int>(sampleRate * duration);
        std::vector<std::complex<float>> signal(N);
        float startFreq = centerFreq - bandwidth / 2.0f;
        float stopFreq  = centerFreq + bandwidth / 2.0f;
        float chirpRate = 0.2f * (stopFreq - startFreq) / duration;

        float noise = 0.02f;
        for (int n = 0; n < N; n++)
        {
            float envelope = std::sin(M_PI * n / N);  // smooth fade in/out
            float t = n / sampleRate;
            float phase = 2.0f * M_PI * (startFreq * t + 0.5f * chirpRate * t * t);
            signal[n] = envelope * std::polar(1.0f, phase);
            signal[n] += std::complex<float>(
                noise * ((rand() / (float)RAND_MAX) - 0.5f),
                noise * ((rand() / (float)RAND_MAX) - 0.5f)
                );
        }

        return signal;
    }

signals:
    void recordingLoaded();

private:
    QMutex m_mutex;
    std::unique_ptr<sigmf_io::Recording> m_recording;
    double m_sampleRate = 0;
    int64_t m_totalSamples = 0;
    int m_fftSize = 1024;   // tune / expose as Q_PROPERTY for zoom later
    int m_hopSize = 1024;   // == fftSize => no overlap, to start

    static void padBottomWithBlack(QImage &img, int firstBlackRow) {
        if (firstBlackRow >= img.height()) return;
        firstBlackRow = std::max(0, firstBlackRow);
        QPainter p(&img);
        p.fillRect(0, firstBlackRow, img.width(), img.height() - firstBlackRow, Qt::black);
    }
};

#endif // IMAGEBACKEND_H
