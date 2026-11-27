#include "backend/spectrogram_builder.h"

#include <QImage>
#include <QDebug>
#include <QString>
#include <kiss_fft.h>
#include <algorithm>
#include <cmath>
#include <complex>
#include <vector>
#include <stdexcept>
#include <memory>
#include <utility>
#include <optional>
#include <tuple>  // for std::ignore

#include "backend/colormaps.h"
#include "backend/tools.h"


namespace burstrf {

SpectrogramBuilder::SpectrogramBuilder()
{}


SpectrogramBuilder::SpectrogramBuilder(std::shared_ptr<const sigmf_io::Recording> recording)
    : recording_(std::move(recording))
{}


SpectrogramBuilder::SpectrogramBuilder(std::shared_ptr<const sigmf_io::Recording> recording, Params params)
    : recording_(std::move(recording)),
    params_(params)
{}


void SpectrogramBuilder::setRecording(std::shared_ptr<const sigmf_io::Recording> recording)
{
    this->recording_ = std::move(recording);
}


// clears away the currently used recording pointer
void SpectrogramBuilder::clearRecording()
{
    this->recording_.reset();
}

// Used by QML frontend to know how many spectrogram chunk objects to put in the scrollable window (flickable).
// TODO: handle channels & invariants properly!
// NOTE: should be on SpectrogramBuilder class?
size_t SpectrogramBuilder::chunkCount() const
{
    if (!recording_) return SpectrogramBuilder::DEFAULT_CHUNK_COUNT;
    return burstrf::ceilDiv<size_t>(this->recording_->size(1), this->params_.chunk_size());
}


// Used by QML to get the total number of rows for the spectrogram view.
// NOTE: when spectrogram uses noverlap, this logic will need to be updated...
size_t SpectrogramBuilder::rowCount() const
{
    // If no recording loaded, use 3 placeholders' worth of expected rows.
    if (!recording_)
        return SpectrogramBuilder::DEFAULT_CHUNK_COUNT * this->params_.spectrogram_chunk_height();

    return burstrf::ceilDiv<size_t>(this->recording_->size(1), this->params_.spectrogram_chunk_width());
}


// returns the sample_start, sample_count corresponding to a given chunk index, based on the chunk_size parameter.
std::optional<std::pair<size_t, size_t>> SpectrogramBuilder::chunkIndexToSampleBounds(const size_t chunk_index) const
{
    // if there's no recording, return empty optional.
    if (!this->recording_) return std::nullopt;

    // convert the chunk_index to a sample_start, based on the chunk_size
    size_t sample_start = this->params_.chunk_size() * chunk_index;

    size_t available = this->recording_->size(1) - sample_start;

    // if no samples available (past EOF), return empty optional.
    if (available <= 0) return std::nullopt;

    // number of sample to read from the file.
    size_t sample_count = std::min(this->params_.chunk_size(), available);
    return {{sample_start, sample_count}};
}


// Returns either a placeholder of a spectrogram for a given chunk.
// TODO: integrate other Param values: noverlap, nfft, etc...
QImage SpectrogramBuilder::generateSpectrogramChunk(const size_t chunk_index) const
{
    // if there's no recording, return placeholder.
    if (!this->recording_) return this->generatePlaceholder();

    // get the number of samples to read - if no more samples available, return placeholder.
    std::optional<std::pair<size_t, size_t>> bounds = this->chunkIndexToSampleBounds(chunk_index);
    if(!bounds) return this->generatePlaceholder();

    // read samples from the file.
    size_t sample_start = bounds.value().first;
    size_t sample_count = bounds.value().second;
    const std::vector<std::complex<double>> samples = this->recording_->get_samples<std::complex<double>>(sample_start, sample_count, 1);

    // Calculate output image dimensions -> nperseg (width) x rows (height)
    // rows = samples.size() / nperseg (ceiling division so sample values are not missed).
    const size_t width = this->params_.nperseg(); // also the column count
    const size_t height = burstrf::ceilDiv<size_t>(samples.size(), width); // also the row count

    // Build an Image Buffer with this size.
    QImage spectrogram(width, height, QImage::Format_RGB32);

    // Initialize kissfft params
    std::vector<kiss_fft_cpx> in(width); // input sample buffer
    std::vector<kiss_fft_cpx> out(width); // output sample buffer

    // TODO: move this to a constructor? not sure if I can...
    // Arguments: FFT size, inverse flag (0 = forward), no custom memory, no size output.
    kiss_fft_cfg cfg = kiss_fft_alloc(width, /*inverse=*/0, nullptr, nullptr); // configures fft params.

    // used to map spectrogram fft row/col values to colors based on dB.
    std::vector<double> db_values(width * height);   // dB for every pixel in a 1D array.
    db_values.reserve(width * height);

    // ---- Pass 1: FFT every row, store dB ----
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

        // calculate dB values
        for (size_t f = 0; f < width; ++f) {    // f = 0 is left (low frequency)
            double re = out[f].r / width;
            double im = out[f].i / width;
            double power = re * re + im * im;   // power = (magnitude)^2
            // power -> dB function
            db_values[t * width + f] = 10.0 * std::log10(power + this->params_.epsilon());
        }
    }

    // release the kiss_fft_cfg... may move this to a destructor later...
    kiss_fft_free(cfg);

    // Set image pixel values based on fft output & color map.
    // t = 0 at top, time increases downward.
    // ---- Pass 2: color the pixels ----
    for (size_t t = 0; t < height; ++t)
    {
        // QImage.scanLine(t) grabs a pointer to a row of pixels (ie. one fft's worth of pixels).
        QRgb *line = reinterpret_cast<QRgb*>(spectrogram.scanLine(t));
        for (size_t f = 0; f < width; ++f)
            // t * width -> gets us to the right row,
            // + f -> gets us to the right column.
            line[f] = this->colormap(db_values[t * width + f]);
    }

    return spectrogram;
}


// sets floor_db and ceil_db values to scale spectrogram values.
void SpectrogramBuilder::computeSetDbScale()
{
    // exit early if recording is empty
    if(!this->recording_) return;

    // NOTE: Hardcoded to 1st channel for now...
    // get the number of samples to read - if no more samples available, exit early.
    std::optional<std::pair<size_t, size_t>> bounds = this->chunkIndexToSampleBounds(0);
    if(!bounds) return;

    // read samples from the file.
    size_t sample_start = bounds.value().first;
    size_t sample_count = bounds.value().second;
    const std::vector<std::complex<double>> samples = this->recording_->get_samples<std::complex<double>>(sample_start, sample_count, 1);

    const size_t width = this->params_.spectrogram_chunk_width();
    if (width == 0 || samples.empty())
        return;

    const size_t frames = samples.size() / width + (samples.size() % width != 0 ? 1 : 0);

    // Collect dB for every bin of every frame (same math as generateSpectrogram).
    std::vector<double> db_values;
    db_values.reserve(frames * width);

    std::vector<kiss_fft_cpx> in(width);
    std::vector<kiss_fft_cpx> out(width);
    kiss_fft_cfg cfg = kiss_fft_alloc(width, /*inverse=*/0, nullptr, nullptr);

    for (size_t t = 0; t < frames; ++t)
    {
        const size_t start = t * width;
        for (size_t i = 0; i < width; ++i)
        {
            std::complex<double> s = (start + i < samples.size())
            ? samples[start + i]
            : std::complex<double>{};
            in[i].r = static_cast<kiss_fft_scalar>(s.real());
            in[i].i = static_cast<kiss_fft_scalar>(s.imag());
        }

        kiss_fft(cfg, in.data(), out.data());

        // The FFT shift doesn't change the set of values, so it can be skipped here.
        for (size_t f = 0; f < width; ++f)
        {
            double re = out[f].r / width;
            double im = out[f].i / width;
            double power = re * re + im * im;
            db_values.push_back(10.0 * std::log10(power + this->params_.epsilon()));
        }
    }

    kiss_fft_free(cfg);

    // Percentile of the collected dB values. nth_element reorders the vector,
    // which is fine since we only need order statistics.
    auto percentile = [&](double p) {
        size_t k = static_cast<size_t>(p * (db_values.size() - 1));
        std::nth_element(db_values.begin(), db_values.begin() + k, db_values.end());
        return db_values[k];
    };

    std::ignore = this->params_.set_floor_db(percentile(this->params_.floor_percentile()) - this->params_.floor_margin_db());
    std::ignore = this->params_.set_ceil_db(percentile(this->params_.ceil_percentile()) + this->params_.ceil_margin_db());
}


// Builds a placeholder chunk for when there's no recording.
QImage SpectrogramBuilder::generatePlaceholder() const
{
    const int chunkW = this->params_.spectrogram_chunk_width();
    const int chunkH = this->params_.spectrogram_chunk_height();
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


// converts a dB value to an RGB color value.
QRgb SpectrogramBuilder::colormap(double db) const
{
    // TODO: rather than handle a bad range, make Params throw/handle a bad range by itself.
    const double span = std::max(params_.ceil_db() - params_.floor_db(), this->params_.epsilon());
    double t = (db - params_.floor_db()) / span;

    if (std::isnan(t))          // clamp() doesn't handle NaN; a bad index would read out of bounds
        t = 0.0;
    t = std::clamp(t, 0.0, 1.0);

    // TODO: add correct mapping to actual colormaps, currently hardcoded to VIRIDIS...
    constexpr size_t kLastIndex = colormaps::VIRIDIS.size() - 1;   // 255
    return colormaps::VIRIDIS[static_cast<size_t>(std::lround(t * kLastIndex))];
}


// TODO: is this correct, if I don't provide the center frequency as a param? What exactly is being computed here?...
double SpectrogramBuilder::binIndexToFrequencyCenter(const unsigned int bin_index, const double sample_rate) const
{
    if (bin_index >= this->params_.nfft()) throw std::invalid_argument("binIndex must be less than the max index value, nfft.");

    // return the center frequency given the index of the bin, using the sample rate.
    return (static_cast<double>(bin_index) * sample_rate) / static_cast<double>(this->params_.nfft());
}


bool SpectrogramBuilder::saveAsPNG(const QImage& image, const QString& path)
{
    if (image.isNull()) {
        qWarning() << "saveAsPNG: image is null, nothing to save";
        return false;
    }
    if (!image.save(path, "PNG")) {
        qWarning() << "saveAsPNG: failed to write" << path;
        return false;
    }
    return true;
}

} // end burstrf namespace
