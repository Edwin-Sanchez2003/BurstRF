#include "SpectrogramBuilder.h"

#include <QImage>
#include <QString>
#include <QDebug>
#include <kiss_fft.h>
#include <algorithm>
#include <cmath>
#include <complex>
#include <vector>
#include <stdexcept>
#include "colormaps.h"


SpectrogramBuilder::SpectrogramBuilder()
    : params_(Params())
{}


SpectrogramBuilder::SpectrogramBuilder(Params params)
    : params_(params)
{}


// re-write this, but as straight-forward as possible.
// NOTE: What are valid return values? Empty Image? etc...
// TODO: integrate other Param values: noverlap, nfft, etc...
QImage SpectrogramBuilder::generateSpectrogram(const std::vector<std::complex<double>>& samples)
{
    // Calculate output image dimensions -> nperseg (width) x rows (height)
    // rows = samples.size() / nperseg (ceiling division so sample values are not missed).
    const size_t width = this->params_.nperseg; // also the column count
    const size_t height = samples.size() / width + (samples.size() % width != 0 ? 1 : 0); // also the row count

    // Build an Image Buffer with this size.
    QImage spectrogram(width, height, QImage::Format_RGB32);

    // Initialize kissfft params
    std::vector<kiss_fft_cpx> in(width); // input sample buffer
    std::vector<kiss_fft_cpx> out(width); // output sample buffer
    // Arguments: FFT size, inverse flag (0 = forward), no custom memory, no size output.
    kiss_fft_cfg cfg = kiss_fft_alloc(width, /*inverse=*/0, nullptr, nullptr); // configures fft params.

    // Set image pixel values based on fft output & color map.
    // t = 0 at top, time increases downward.
    for (size_t t = 0; t < height; ++t)
    {
        // start index for samples slice
        const size_t start = t * width;

        // Copy the slice of samples into `in`, converting each std::complex<double>
        // to a kiss_fft_cpx. Zero-pad if the slice runs past the end of samples.
        for (size_t i = 0; i < width; ++i)
        {
            std::complex<double> s = (start + i < samples.size())
                ? samples[start + i]
                : std::complex<double>{};
            in[i].r = static_cast<kiss_fft_scalar>(s.real());
            in[i].i = static_cast<kiss_fft_scalar>(s.imag());
        }

        // performs fft on the in vector we just populated, and puts the fft values in out.
        kiss_fft(cfg, in.data(), out.data());

        // perform fft shift to get correct placement of frequency bins
        // negative frequencies on the left and positive on the right, centered on bin 0.
        std::rotate(out.begin(), out.begin() + (width + 1) / 2, out.end());

        // QImage.scanLine(t) grabs a pointer to a row of pixels (ie. one fft's worth of pixels).
        QRgb *line = reinterpret_cast<QRgb*>(spectrogram.scanLine(t));
        for (size_t f = 0; f < width; ++f) {                        // f = 0 is left (low frequency)
            double re = out[f].r / width;
            double im = out[f].i / width;
            double power = re * re + im * im;                                   // power = magnitude squared, no sqrt
            double db    = 10.0 * std::log10(power + this->params_.epsilon);    // convert to decibel scale
            line[f] = colormap(db);     // color for magnitude of bin f at time t
        }
    }

    return spectrogram;
}


// converts a dB value to an RGB color value.
QRgb SpectrogramBuilder::colormap(double db) const
{
    // TODO: rather than handle a bad range, make Params throw/handle a bad range by itself.
    const double span = std::max(params_.ceil_db - params_.floor_db, this->params_.epsilon);
    double t = (db - params_.floor_db) / span;

    if (std::isnan(t))          // clamp() doesn't handle NaN; a bad index would read out of bounds
        t = 0.0;
    t = std::clamp(t, 0.0, 1.0);

    // TODO add correct mapping to actual colormaps, currently hardcoded to VIRIDIS...
    constexpr size_t kLastIndex = colormaps::VIRIDIS.size() - 1;   // 255
    return colormaps::VIRIDIS[static_cast<size_t>(std::lround(t * kLastIndex))];
}


// TODO: is this correct, if I don't provide the center frequency as a param? What exactly is being computed here?...
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
