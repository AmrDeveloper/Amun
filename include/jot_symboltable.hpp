#pragma once

#include <any>
#include <map>
#include <memory>
#include <optional>

class JotSymbolTable {
  public:
    JotSymbolTable() {}

    JotSymbolTable(std::optional<std::shared_ptr<JotSymbolTable>> parent)
        : parent_symbol_table(parent) {}

    bool define(std::string name, std::any value);

    bool is_defined(std::string name);

    std::any lookup(std::string name);

  private:
    std::map<std::string, std::any> environment;
    std::optional<std::shared_ptr<JotSymbolTable>> parent_symbol_table;
};