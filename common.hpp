#ifndef COMMON_HPP
#define COMMON_HPP

#include <cmath>

enum Status {
    FAIL,
    SUCCESS
};

template <typename T>
static bool decimalCmp(T a, T b)
{
  return (fabs(a - b) <= std::numeric_limits<T>::epsilon() * std::max(fabs(a), fabs(b)));
}

#endif