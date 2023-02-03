#include "jot_basic.hpp"

int64 str_to_int(const char* p)
{
    int64 value = 0;
    bool  neg = false;

    if (*p == '-') {
        neg = true;
        ++p;
    }

    while (*p >= '0') {
        value = (value * 10) + (*p - '0');
        ++p;
    }

    return neg ? -value : value;
}

float64 str_to_float(const char* p) { return std::atof(p); }
