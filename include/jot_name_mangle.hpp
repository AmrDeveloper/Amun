#pragma once

#include "jot_type.hpp"

#include <string>
#include <vector>

auto mangle_type(Shared<JotType> type) -> std::string;

auto mangle_tuple_type(Shared<JotTupleType> type) -> std::string;

auto mangle_types(std::vector<Shared<JotType>> types) -> std::string;