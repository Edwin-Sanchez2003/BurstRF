#ifndef SPECTROGRAMBUILDER_H
#define SPECTROGRAMBUILDER_H

#include <cstdint>
#include <vector>
#include <complex>
#include <QImage>

/*
 * SpectrogramBuilder
 *
 * Builds spectrogram chunks as QImages given a vector of complex samples,
 * using the given parameters.
 *
 * TODO:
 * Add:
 * * Struct to hold most settings.
 * * Window (Hann, Blackman-Harris)
 * * Colormap (256-entry look-up table viridis, inferno.
 * * Color scale (dynamic range) - find the min/max power, use as the scale. needs to be member variables so things can be adjusted later.
 * * Epsilon - added before applying the log function so 0 doesn't map to infinity.
 * * Image dimensions should be fixed-sized, based on the builder params. Let other outside components resize the image for the screen.
 * * Orientation - allows you to pick between horizontal or vertical orientations.
 *
 * NOTE: This may need to evolve into a spectrogram class - if a user wants to edit spectrogram settings on-the-fly,
 *      then we will need a buffer of the spectrogram output so we can re-compute the image without re-running an FFT...
 *      Memory vs. speed problem...
 */

class SpectrogramBuilder
{
public:

    // Whether the output spectrogram should be a horizontal scroll or vertical scroll.
    enum class Orientation { HORIZONTAL, VERTICAL };
    // Transformations common to improve signal quality when visualizing.
    enum class WindowType { NONE, HANN, BLACKMANHARRIS };
    // When nfft is much wider than the visual plot, average or max to merge fft bins to not lose some signals.
    enum class BinReduction { NONE, MAX, AVERAGE };

    // TODO: Force parameters to be initialized to valid values at construction
    struct Params
    {
        size_t nperseg = 1024;                // Number of samples that get processed per FFT computation.
        std::optional<size_t> noverlap;       // Number of samples to overlap each FFT computation. (unset -> nperseg / 2)
        std::optional<size_t> nfft;           // Size of FFT if zero-padding is desired, otherwise same as nperseg. (unset -> nperseg)
        WindowType window_type = WindowType::HANN;
        double epsilon = 1e-12;                     // -120 dB floor before the log
        double floor_db = 0.0;
        double ceil_db = 100.0;
        bool auto_range = false;                    // if true, floor/ceil are computed from data
        Orientation orientation = Orientation::VERTICAL;
        size_t bin_reduction_factor = 1;      // merge N adjacent bins; 1 = off
        BinReduction bin_reduction_mode = BinReduction::NONE;

        // Resolved values (these replace your TODO comments)
        size_t effective_noverlap() const { return noverlap.value_or(nperseg / 2); }
        size_t effective_nfft() const     { return std::max(nfft.value_or(nperseg), nperseg); }
        size_t hop() const                { return nperseg - effective_noverlap(); }

        // TODO: Colormap? Not sure how to do that...
    };

public:
    // TODO: write constructors with the major members defined, and use SpectrogramBuilder::Params implicitly.
    // Catch-all constructor that just takes in a SpectrogramBuilder::Params struct.
    SpectrogramBuilder();
    explicit SpectrogramBuilder(Params params);

    Params getParams() const { return this->params_; }
    void setParams(Params params) { this->params_ = params; }

    // generates a spectrogram using the configured member variables.
    QImage generateSpectrogram(const std::vector<std::complex<double>>& samples);

    // Compute initial db scale values, so that spectrogram color scaling works visually.
    // NOTE: This should end up a function on Params...
    // NOTE: Should make another version that is given sample indices, so we can calculate
    // across multiple chunks of a file without reading the whole file into memory...
    // TODO: should set this->params_.floor_db, this->params_.ceil_db values.
    void computeDbScale(const std::vector<std::complex<double>>& samples);

    // Helper to identify what frequency a certain image index corresponds to.
    // NOTE: depending on scaling in actual viewer, may not be accurate...
    double binIndexToFrequencyCenter(const unsigned int bin_index, const double sample_rate);

    // helper function to save the spectrogram for independent evaluation.
    bool saveAsPNG(const QImage& image, const QString& path);

private:
    Params params_;
};

#endif // SPECTROGRAMBUILDER_H
