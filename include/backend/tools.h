#ifndef BURSTRF_TOOLS_H
#define BURSTRF_TOOLS_H

namespace burstrf {

// overflow-safe ceiling division for positive integer values
template<typename T>
T ceilDiv(T numerator, T denominator)
{
    return numerator / denominator + (numerator % denominator != 0 ? 1 : 0);
}

} // end burstrf namespace

#endif // BURSTRF_TOOLS_H
