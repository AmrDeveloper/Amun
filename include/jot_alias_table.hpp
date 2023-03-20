#pragma once

#include "jot_primitives.hpp"

#include <unordered_map>

class JotAliasTable {
  public:
    JotAliasTable();

    auto define_alias(std::string alias, Shared<JotType> type) -> void;

    auto resolve_alias(std::string alias) -> Shared<JotType>;

    auto contains(std::string alias) -> bool;

  private:
    auto config_type_alias_table() -> void;

    std::unordered_map<std::string, Shared<JotType>> type_alias_table;
};