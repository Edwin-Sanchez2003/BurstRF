#include "params.h"

#include <limits>
#include <expected>
#include <string>

namespace burstrf {


std::expected<void, std::string> Params::set_nperseg(size_t nperseg)
{
    // Must be at least one or greater
    if(nperseg < 1)
        return std::unexpected("nperseg must be at least one. nperseg: " + std::to_string(nperseg));
    this->nperseg_ = nperseg;
    return {};
}

std::expected<void, std::string> Params::set_noverlap(size_t noverlap)
{
    // Must be within [0, nperseg)
    if(noverlap < 0 || noverlap >= this->nperseg_)
        return std::unexpected("noverlap must be within [0, nperseg). noverlap: " + std::to_string(noverlap));
    this->noverlap_ = noverlap;
    return {};
}

std::expected<void, std::string> Params::set_nfft(size_t nfft)
{
    // Must be greater than or equal to nperseg
    if(nfft < this->nperseg_)
        return std::unexpected("nfft must be equal to or greater than nperseg. nfft: " + std::to_string(nfft));
    this->nfft_ = nfft;
    return {};
}

std::expected<void, std::string> Params::set_epsilon(double epsilon)
{
    // Must be a positive value
    // NOTE: may later want to set a max value to avoid drowning out the signal...
    if (!std::isfinite(epsilon) || epsilon <= 0.0)
        return std::unexpected("epsilon must be a finite, strictly positive value. epsilon: " + std::to_string(epsilon));
    this->epsilon_ = epsilon;
    return {};
}

std::expected<void, std::string> Params::set_floor_db(double floor_db)
{
    // Must be less than ceil_db
    if(floor_db >= this->ceil_db_)
        return std::unexpected("floor_db must be less than ceil_db. floor_db: " + std::to_string(floor_db));
    this->floor_db_ = floor_db;
    return {};
}

std::expected<void, std::string> Params::set_ceil_db(double ceil_db)
{
    // Must be greater than floor_db
    if(ceil_db <= this->floor_db_)
        return std::unexpected("ceil_db must be greater than floor_db. ceil_db: " + std::to_string(ceil_db));
    this->ceil_db_ = ceil_db;
    return {};
}

std::expected<void, std::string> Params::set_floor_percentile(double floor_percentile)
{
    // Must be within [0.0, 100.0], and less than ceil_percentile.
    if(floor_percentile >= this->ceil_percentile_ || (floor_percentile < 0.0) || floor_percentile > 100.0)
        return std::unexpected(
            "floor_percentile must be less than ceil_percentile, and within [0.0, 100.0]. floor_percentile: " + std::to_string(floor_percentile));
    this->floor_percentile_ = floor_percentile;
    return {};
}

std::expected<void, std::string> Params::set_ceil_percentile(double ceil_percentile)
{
    // Must be within [0.0, 100.0], and greater than ceil_percentile.
    if(ceil_percentile <= this->floor_percentile_ || (ceil_percentile < 0.0) || ceil_percentile > 100.0)
        return std::unexpected(
            "ceil_percentile must be less than floor_percentile, and within [0.0, 100.0]. ceil_percentile: " + std::to_string(ceil_percentile));
    this->ceil_percentile_ = ceil_percentile;
    return {};
}

std::expected<void, std::string> Params::set_floor_margin_db(double floor_margin_db)
{
    // Must be a non-negative value
    if(floor_margin_db < 0.0)
        return std::unexpected("floor_margin_db must be greater than or equal to 0.0. floor_margin_db: " + std::to_string(floor_margin_db));
    this->floor_margin_db_ = floor_margin_db;
    return {};
}

std::expected<void, std::string> Params::set_ceil_margin_db(double ceil_margin_db)
{
    // Must be a non-negative value
    if(ceil_margin_db < 0.0)
        return std::unexpected("ceil_margin_db must be greater than or equal to 0.0. ceil_margin_db: " + std::to_string(ceil_margin_db));
    this->ceil_margin_db_ = ceil_margin_db;
    return {};
}

std::expected<void, std::string> Params::set_bin_reduction_factor(size_t bin_reduction_factor)
{
    // Must be at least one or greater
    if(bin_reduction_factor < 1)
        return std::unexpected("bin_reduction_factor must be at least one. bin_reduction_factor: " + std::to_string(bin_reduction_factor));
    this->bin_reduction_factor_ = bin_reduction_factor;
    return {};
}

} // end burstrf namespace
