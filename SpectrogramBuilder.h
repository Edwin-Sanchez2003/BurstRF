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
 */

class SpectrogramBuilder
{
public:

    // Whether the output spectrogram should be a horizontal scroll or vertical scroll.
    enum class Orientation
    {
        HORIZONTAL,
        VERTICAL
    };

    struct Params
    {
    public:
        double sample_rate = 1.0;
        unsigned int nperseg = 1024;
        unsigned int noverlap = 512; // TODO: make this set by a const function initialized as half nperseg.
        unsigned int nfft = 1024;   // TODO: make this set as nperseg by default, must be greater than or equal to nperseg.
        std::string window_type = "Hann"; // TODO: implement Hann & Blackman-Harris window functions.
        double epsilon = 0.0001;    // TODO: pick a good default value to avoid log-forced infinities...
        double floor_db = 0.0;  // TODO: make a function to compute floor & ceil db. Also, pick decent defaults.
        double ceil_db = 0.0;
        Orientation orientation = Orientation::VERTICAL; // Default is Waterfall plot.
        double bin_reduction_ratio = 1.0; // When nfft is much wider than the visual plot, average or max to merge fft bins to not lose some signals.
        // TODO: Colormap? Not sure how to do that...
    };

public:
    // TODO: write constructors with the major members defined, and use SpectrogramBuilder::Params implicitly.
    // Catch-all constructor that just takes in a SpectrogramBuilder::Params struct.
    SpectrogramBuilder(double sampleRate);
    SpectrogramBuilder(double sampleRate, unsigned int nperseg, unsigned int noverlap, unsigned int nfft);

    // Main workhorse: generates a spectrogram using the configured member variables.
    QImage generateSpectrogram(const std::vector<std::complex<double>>& samples);

    // Helper to identify what frequency a certain image index corresponds to.
    // NOTE: depending on scaling in actual viewer, may not be accurate...
    double binIndexToFrequencyCenter(const unsigned int binIndex);

    /*
     * Getters/Setters
     */
    double getSampleRate() const;
    unsigned int getNperseg() const;
    unsigned int getNoverlap() const;
    unsigned int getNfft() const;
    void setSampleRate(double sampleRate);
    void setParams(unsigned int nperseg, unsigned int noverlap, unsigned int nfft);

private:
    double sampleRate;      // rate at which samples were captured. Needed to compute bins.
    unsigned int nperseg;   // Number of samples that get FFT computed on per FFT computation.
    unsigned int noverlap;  // Number of samples to overlap each FFT computation.
    unsigned int nfft;      // Size of FFT (if zero-padding is desired, otherwise same as nperseg)

    const unsigned int DEFAULT_NPERSEG = 1024;
};

#endif // SPECTROGRAMBUILDER_H
