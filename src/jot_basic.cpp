#include "../include/jot_basic.hpp"

auto str_to_int(const char* p) -> int64
{
    int64 value = 0;
    bool neg = false;

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

auto str_to_float(const char* p) -> float64 { return std::atof(p); }

auto is_ends_with(const std::string& str, const std::string& part) -> bool
{
    return str.find(part, str.size() - part.size()) != std::string::npos;
}