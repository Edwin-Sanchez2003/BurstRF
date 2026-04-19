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


QImage SpectrogramBuilder::generateSpectrogram(std::vector<std::complex<double>> samples)
{
    // NOTE: if samples is less than nperseg per fft row generated, zero-pad w/nfft value!!!
}


double SpectrogramBuilder::binIndexToFrequencyCenter(unsigned int binIndex)
{
    if (binIndex > this->nfft) throw std::invalid_argument("binIndex must be less than the max index value, nfft.");
    // TODO: return the center frequency given the index of the bin, using the sample rate.
    return (static_cast<double>(binIndex) * this->sampleRate) / static_cast<double>(this->nfft);
}
