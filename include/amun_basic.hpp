#pragma once

#include <algorithm>
#include <iostream>
#include <memory>
#include <vector>

// Language Information
#define AMUN_LANGUAGE_EXTENSION ".amun"
#define AMUN_LANGUAGE_VERSION "0.0.1"
#define AMUN_LIBRARIES_PREFIX "../lib/"

// Singed integers types
using int8 = std::int8_t;
using int16 = std::int16_t;
using int32 = std::int32_t;
using int64 = std::int64_t;

// Un Singed integers types
using uint8 = std::uint8_t;
using uint16 = std::uint16_t;
using uint32 = std::uint32_t;
using uint64 = std::uint64_t;

// Float types
using float32 = float;
using float64 = double;

// Smart pointers types
template <class T>
using Shared = std::shared_ptr<T>;

template <class T>
using Unique = std::unique_ptr<T>;

// Compare two char* with length of 2
#define str2Equals(first, other) first[0] == other[0] && first[1] == other[1]

// Compare two char* with length of 3
#define str3Equals(first, other) str2Equals(first, other) && first[2] == other[2]

// Compare two char* with length of 4
#define str4Equals(first, other) str3Equals(first, other) && first[3] == other[3]

// Compare two char* with length of 5
#define str5Equals(first, other) str4Equals(first, other) && first[4] == other[4]

// Compare two char* with length of 6
#define str6Equals(first, other) str5Equals(first, other) && first[5] == other[5]

// Compare two char* with length of 7
#define str7Equals(first, other) str6Equals(first, other) && first[6] == other[6]

// Compare two char* with length of 8
#define str8Equals(first, other) str7Equals(first, other) && first[7] == other[7]

// Compare two char* with length of 9
#define str9Equals(first, other) str8Equals(first, other) && first[8] == other[8]

// Compare two char* with length of 10
#define str10Equals(first, other) str9Equals(first, other) && first[9] == other[9]

// Compare two char* with length of 11
#define str11Equals(first, other) str10Equals(first, other) && first[10] == other[10]

// Convert string to integer
auto str_to_int(const char* p) -> int64;

// Convert string to float
auto str_to_float(const char* p) -> float64;

auto is_ends_with(const std::string& str, const std::string& part) -> bool;

// Return true if element is in the vector
template <typename T>
auto is_contains(const std::vector<T>& vec, const T& element) -> bool
{
    return std::find(vec.begin(), vec.end(), element) != vec.end();
}

// Return the index of element or -1 if not exists
template <typename T>
auto index_of(const std::vector<T>& vec, const T& element) -> int
{
    auto it = std::find(vec.begin(), vec.end(), element);
    return it == vec.end() ? -1 : it - vec.begin();
}

// Append vector elements at the end of another vector
template <typename T>
auto append_vectors(std::vector<T>& vec, std::vector<T>& extra) -> void
{
    vec.reserve(vec.size() + extra.size());
    for (auto element : extra) {
        vec.push_back(element);
    }
}

template <typename... Args>
std::string string_fmt(const char* fmt, Args... args)
{
    size_t size = snprintf(nullptr, 0, fmt, args...);
    std::string buf;
    buf.reserve(size + 1);
    buf.resize(size);
    snprintf(buf.data(), size + 1, fmt, args...);
    return buf;
}