#include "SpectrogramBuilder.h"

#include <vector>
#include <complex>
#include <stdexcept>
#include <QImage>
#include <kissfft.hh>


SpectrogramBuilder::SpectrogramBuilder(Params params)
    : params_(params)
{}


// sample_rate: rate at which samples were captured. Needed to compute bins.
QImage SpectrogramBuilder::generateSpectrogram(const double sample_rate, const std::vector<std::complex<double>>& samples)
{
    unsigned int numRows = (samples.size() + this->nfft - 1) / this->nfft;

    QImage imageBuffer(numRows, this->nfft, QImage::Format_Grayscale8);

    kissfft<double> fft(this->nfft, false);

    std::vector<std::vector<double>> magnitudes(numRows, std::vector<double>(this->nfft));
    double maxMag = 0.0;

    // TODO: use noverlap properly — currently steps by nfft with no overlap.
    for (unsigned int i = 0; i < numRows; ++i) {
        auto start = samples.begin() + i * this->nfft;
        auto end = std::min(start + this->nfft, samples.end());

        std::vector<std::complex<double>> chunk(start, end);
        chunk.resize(this->nfft); // zero-pads if short

        std::vector<std::complex<double>> output(this->nfft);
        fft.transform(chunk.data(), output.data());

        for (unsigned int j = 0; j < this->nfft; ++j) {
            double magnitude = std::abs(output[j]);
            magnitudes[i][j] = magnitude;
            maxMag = std::max(maxMag, magnitude);
        }
    }

    // Convert to grayscale pixels (simple log-scaled normalization)
    for (unsigned int i = 0; i < numRows; ++i) {
        for (unsigned int j = 0; j < this->nfft; ++j) {
            double mag = magnitudes[i][j];
            double norm = (maxMag > 0.0) ? mag / maxMag : 0.0;
            int gray = static_cast<int>(std::clamp(norm * 255.0, 0.0, 255.0));
            // row 0 of the image = bin 0; flip if you want low freq at bottom
            imageBuffer.setPixel(i, j, qRgb(gray, gray, gray));
        }
    }

    return imageBuffer;
}


double SpectrogramBuilder::binIndexToFrequencyCenter(unsigned int binIndex)
{
    if (binIndex >= this->nfft) throw std::invalid_argument("binIndex must be less than the max index value, nfft.");

    // return the center frequency given the index of the bin, using the sample rate.
    return (static_cast<double>(binIndex) * this->sampleRate) / static_cast<double>(this->nfft);
}
