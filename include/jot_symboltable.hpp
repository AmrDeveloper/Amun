#pragma once

#include <any>
#include <map>
#include <memory>

class JotSymbolTable {
  public:
    JotSymbolTable() = default;

    JotSymbolTable(std::shared_ptr<JotSymbolTable> parent) : parent_symbol_table(parent) {}

    bool define(const std::string &name, std::any value);

    bool update(const std::string &name, std::any value);

    bool is_defined(const std::string &name);

    std::any lookup(const std::string &name);

    std::shared_ptr<JotSymbolTable> get_parent_symbol_table() { return parent_symbol_table; }

  private:
    std::map<std::string, std::any> environment;
    std::shared_ptr<JotSymbolTable> parent_symbol_table;
};