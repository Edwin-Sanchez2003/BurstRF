#ifndef BURSTRF_SPECTROGRAMBUILDER_H
#define BURSTRF_SPECTROGRAMBUILDER_H

#include <array>
#include <QRgb>
#include <cstdint>
#include <vector>
#include <complex>
#include <QImage>
#include "params.h"

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

namespace burstrf {

class SpectrogramBuilder
{
public:

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
    // NOTE: Currently integrated with generateSpectrogram - a tightly coupled step with the algorithm currently...
    //       Will need to untangle later...
    //void computeDbScale(const std::vector<std::complex<double>>& samples);

    // gets the corresponding color given a dB value
    QRgb colormap(double db) const;

    // Helper to identify what frequency a certain image index corresponds to.
    // NOTE: depending on scaling in actual viewer, may not be accurate...
    double binIndexToFrequencyCenter(const unsigned int bin_index, const double sample_rate);

    // helper function to save the spectrogram for independent evaluation.
    bool saveAsPNG(const QImage& image, const QString& path);

private:
    Params params_;
};

} // end burstrf namespace

#endif // BURSTRF_SPECTROGRAMBUILDER_H
