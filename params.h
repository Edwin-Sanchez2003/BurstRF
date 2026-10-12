#ifndef BURSTRF_PARAMS_H
#define BURSTRF_PARAMS_H

#include <optional>
#include <cmath>

namespace burstrf {

// Whether the output spectrogram should be a horizontal scroll or vertical scroll.
enum class Orientation { HORIZONTAL, VERTICAL };
// Transformations common to improve signal quality when visualizing.
enum class WindowType { NONE, HANN, BLACKMANHARRIS };
// When nfft is much wider than the visual plot, average or max to merge fft bins to not lose some signals.
enum class BinReduction { NONE, MAX, AVERAGE };
// Colormap enumerations - helps map certain dB power values to colors for the spectrogram.
enum class ColorMap { VIRIDIS, PLASMA, INFERNO, MAGMA, JET, GREY };

// TODO: Force parameters to be initialized to valid values at construction
// TODO: support setting values past ceil_db to zero, so that it hides high-power signals???
class Params
{
public:
    Params() {} // defaults are valid values.

    // TODO: setters -> do not allow invalid values to be set.

    // getters -> should always return a valid value (setters/contructor do not allow invalid values).
    size_t nperseg()  const { return this->nperseg_; }
    size_t noverlap() const { return this->noverlap_; }
    size_t nfft()     const { return this->nfft_; }
    double epsilon()  const { return this->epsilon_; }
    size_t hop()      const { return this->nperseg() - this->noverlap(); } // how far to slide the window

    bool auto_range = true; // if true, floor/ceil are computed from the data.
    double floor_db() const { return this->floor_db_; }
    double ceil_db()  const { return this->ceil_db_; }
    double floor_percentile() const { return this->floor_percentile_; }
    double ceil_percentile() const { return this->ceil_percentile_; }
    double floor_margin_db() const { return this->floor_margin_db_; }
    double ceil_margin_db() const { return this->ceil_margin_db_; }

    bool reverse_color_map = false; // inverses indices for color maps to flip coloring.
    ColorMap colormap() const { return this->colormap_; }
    WindowType window_type() const { return this->window_type_; }
    Orientation orientation() const { return this->orientation_; }

    size_t bin_reduction_factor() const { return this->bin_reduction_factor_; }
    BinReduction bin_reduction_mode() const { return this->bin_reduction_mode_; }

private:
    size_t nperseg_ = 1024;   // Number of samples that get processed per FFT computation.
    size_t noverlap_ = 512;   // Number of samples to overlap each FFT computation. (unset -> nperseg / 2)
    size_t nfft_ = 1024;      // Size of FFT if zero-padding is desired, otherwise same as nperseg. (unset -> nperseg)
    WindowType window_type_ = WindowType::HANN;
    double epsilon_ = 1e-12;                 // -120 dB floor before the log
    double floor_db_ = 40.0;                 // the lowest dB value allowed - everything above gets rounded up to this value.
    double ceil_db_ = 80.0;                  // the highest dB value allowed - everything above gets rounded down to this value.
    // the percentile used when picking the floor_db (0.05 -> 5% -> 95% of freq bins are louder than this).
    double floor_percentile_ = 0.05;
    // the percentile used when picking the ceil_db (0.99 -> 99% -> 99% of freq bins are quieter than this).
    double ceil_percentile_ = 0.99;
    double floor_margin_db_ = 5.0;           // used with auto_range: offsets the bottom-percentile floor_db to avoid overclipping.
    double ceil_margin_db_ = 3.0;            // used with auto_range: offsets the top-percentile ceil_db to avoid overclipping.
    ColorMap colormap_ = ColorMap::VIRIDIS;  // specifies a certain colormap to use when generating spectrograms.
    Orientation orientation_ = Orientation::VERTICAL;
    size_t bin_reduction_factor_ = 1;        // merge N adjacent bins; 1 = off
    BinReduction bin_reduction_mode_ = BinReduction::NONE;
};

} // end burstrf namespace


#endif // BURSTRF_PARAMS_H
