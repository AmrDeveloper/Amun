#pragma once

#include "amun_type.hpp"

#include <string>
#include <vector>

auto mangle_type(Shared<amun::Type> type) -> std::string;

auto mangle_tuple_type(Shared<amun::TupleType> type) -> std::string;

auto mangle_operator_function(TokenKind kind, Shared<amun::Type> left, Shared<amun::Type> right)
    -> std::string;

auto mangle_types(std::vector<Shared<amun::Type>> types) -> std::string;