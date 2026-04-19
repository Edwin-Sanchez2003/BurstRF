#ifndef SPECTROGRAMBUILDER_H
#define SPECTROGRAMBUILDER_H

#include <vector>
#include <complex>
#include <QImage>

/*
 * SpectrogramBuilder
 *
 * Builds spectrogram chunks as QImages given a vector of complex samples,
 * using the given parameters.
 */

class SpectrogramBuilder
{
public:
    SpectrogramBuilder(double sampleRate);
    SpectrogramBuilder(double sampleRate, unsigned int nperseg, unsigned int noverlap, unsigned int nfft);

    // Main workhorse: generates a spectrogram using the configured member variables.
    QImage generateSpectrogram(const std::vector<std::complex<double>>& samples);

    // Helper to identify what frequency a certain image index corresponds to.
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
