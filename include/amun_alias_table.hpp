#pragma once

#include "amun_primitives.hpp"

#include <unordered_map>

namespace amun {

class AliasTable {
  public:
    AliasTable();

    auto define_alias(std::string alias, Shared<amun::Type> type) -> void;

    auto resolve_alias(std::string alias) -> Shared<amun::Type>;

    auto contains(std::string alias) -> bool;

  private:
    auto config_type_alias_table() -> void;

    std::unordered_map<std::string, Shared<amun::Type>> type_alias_table;
};

} // namespace amun