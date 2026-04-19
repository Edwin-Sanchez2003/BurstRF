#include "SpectrogramBuilder.h"

#include <vector>
#include <complex>
#include <stdexcept>
#include <QImage>
#include <kiss_fft.h>


SpectrogramBuilder::SpectrogramBuilder(double sampleRate)
: nperseg(SpectrogramBuilder::DEFAULT_NPERSEG),
  noverlap(0),
  nfft(SpectrogramBuilder::DEFAULT_NPERSEG)
{
    this->setSampleRate(sampleRate);
}


SpectrogramBuilder::SpectrogramBuilder(double sampleRate, unsigned int nperseg, unsigned int noverlap, unsigned int nfft)
{
    this->setSampleRate(sampleRate);
    this->setParams(nperseg, noverlap, nfft);
}


void SpectrogramBuilder::setSampleRate(double sampleRate)
{
    // sample rate cannot be negative; throw an error.
    if (sampleRate <= 0.0) throw std::invalid_argument("Sample rate must be positive.");
    this->sampleRate = sampleRate;
}


void SpectrogramBuilder::setParams(unsigned int nperseg, unsigned int noverlap, unsigned int nfft)
{
    // nperseg must be greater than zero (at least one).
    // If not, silently replace with 1024.
    if (nperseg <= 0)
    {
        this->nperseg = SpectrogramBuilder::DEFAULT_NPERSEG;
    }

    // noverlap must be between [0, nperseg).
    // Replace with max valid value if above threshold.
    if (noverlap >= this->nperseg)
    {
        this->noverlap = this->nperseg - 1;
    }

    // nfft must be greater than or equal to nperseg.
    // if not, replace with nperseg
    if (nfft < this->nperseg)
    {
        this->nfft = this->nperseg;
    }
}


double SpectrogramBuilder::getSampleRate() const
{
    return this->sampleRate;
}


unsigned int SpectrogramBuilder::getNoverlap() const
{
    return this->noverlap;
}


unsigned int SpectrogramBuilder::getNperseg() const
{
    return this->nperseg;
}


unsigned int SpectrogramBuilder::getNfft() const
{
    return this->nfft;
}


QImage SpectrogramBuilder::generateSpectrogram(const std::vector<std::complex<double>>& samples)
{
    // Loop over ceil(samples.size() / nfft) -> that's the number of fft rows we will generate.
    // Integer division ceiling trick (no floating point needed)
    unsigned int numRows = (samples.size() + this->nfft - 1) / this->nfft;

    // TODO: build QImage buffer to insert rows as they are converted.
    QImage imageBuffer();

    // TODO: use noverlap properly!!!

    // Create a kissfft object for forward transform (false = forward, true = inverse)
    kissfft<double> fft(this->nfft, false);

    // loop over numRows
    for (unsigned int i = 0; i < numRows; ++i) {
        // use iterators to determine when
        auto start = samples.begin() + i * this->nfft;
        auto end = std::min(start + this->nfft, samples.end());

        // Create input vector with complex floats
        std::vector<std::complex<double>> chunk(start, end);
        std::vector<std::complex<double>> output(nfft);

        // If samples is less than nperseg per fft row generated, zero-pad w/nfft value.
        if (chunk.size() < this->nfft) chunk.resize(this->nfft);

        // Perform FFT on chunk
        fft.transform(chunk.data(), output.data());

        // TODO: calculate magnitudes on output

        // TODO: convert magnitudes to color range (RGB pixels)

        // TODO: stick into QImage's buffer.
    }

    return imageBuffer;
}


double SpectrogramBuilder::binIndexToFrequencyCenter(unsigned int binIndex)
{
    if (binIndex > this->nfft) throw std::invalid_argument("binIndex must be less than the max index value, nfft.");
    // TODO: return the center frequency given the index of the bin, using the sample rate.
    return (static_cast<double>(binIndex) * this->sampleRate) / static_cast<double>(this->nfft);
}
