#include "SpectrogramBuilder.h"

#include <QImage>
#include <QString>
#include <QDebug>
#include <kissfft.hh>
#include <algorithm>
#include <cmath>
#include <complex>
#include <vector>
#include <stdexcept>


SpectrogramBuilder::SpectrogramBuilder()
    : params_(Params())
{}


SpectrogramBuilder::SpectrogramBuilder(Params params)
    : params_(params)
{}


// re-write this, but as straight-forward as possible.
QImage SpectrogramBuilder::generateSpectrogram(const std::vector<std::complex<double>>& samples)
{
    // 1. Validate
    const size_t nperseg = this->params_.nperseg;
    if (nperseg == 0 || samples.size() < nperseg) {
        qWarning() << "Not enough samples for one segment:" << samples.size() << "<" << nperseg;
        return QImage();
    }

    // No overlap yet: segments are back to back, so hop == nperseg.
    const size_t num_segments = samples.size() / nperseg;

    // 2. FFT each segment
    kissfft<double> fft(static_cast<int>(nperseg), false);
    std::vector<std::complex<double>> out(nperseg);

    // Waterfall layout: width = frequency bins, height = time (row 0 is the oldest segment).
    QImage image(static_cast<int>(nperseg), static_cast<int>(num_segments),
                 QImage::Format_Grayscale8);

    const double floor_db = this->params_.floor_db;
    const double span = std::max(this->params_.ceil_db - floor_db, 1e-9);
    const double scale = 1.0 / static_cast<double>(nperseg); // so a bin-centered full-scale tone reads ~0 dB

    for (size_t seg = 0; seg < num_segments; ++seg) {
        fft.transform(samples.data() + seg * nperseg, out.data());

        uchar* row = image.scanLine(static_cast<int>(seg));
        for (size_t bin = 0; bin < nperseg; ++bin) {
            // 3. Power -> dB
            const double power = std::norm(out[bin]) * scale * scale;
            const double db = 10.0 * std::log10(power + this->params_.epsilon);

            // 4. dB -> 0..255, clamped to [floor_db, ceil_db]
            const double t = std::clamp((db - floor_db) / span, 0.0, 1.0);

            // fftshift: put DC in the middle of the row
            row[(bin + nperseg / 2) % nperseg] = static_cast<uchar>(std::lround(t * 255.0));
        }
    }
    return image;
}


double SpectrogramBuilder::binIndexToFrequencyCenter(const unsigned int bin_index, const double sample_rate)
{
    if (bin_index >= this->params_.effective_nfft()) throw std::invalid_argument("binIndex must be less than the max index value, nfft.");

    // return the center frequency given the index of the bin, using the sample rate.
    return (static_cast<double>(bin_index) * sample_rate) / static_cast<double>(this->params_.effective_nfft());
}


bool SpectrogramBuilder::saveAsPNG(const QImage& image, const QString& path)
{
    if (image.isNull()) {
        qWarning() << "saveImageToPng: image is null, nothing to save";
        return false;
    }
    if (!image.save(path, "PNG")) {
        qWarning() << "saveImageToPng: failed to write" << path;
        return false;
    }
    return true;
}
