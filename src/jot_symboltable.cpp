#include "../include/jot_symboltable.hpp"

bool JotSymbolTable::define(std::string name, std::any value) {
    if (environment.contains(name)) return false;
    environment[name] = value;
    return true;
}

bool JotSymbolTable::is_defined(std::string name) {
    return environment.contains(name);
}

std::any JotSymbolTable::lookup(std::string name) {
    if (environment.contains(name)) {
        return environment[name];
    }

    if (parent_symbol_table.has_value()) {
        return parent_symbol_table->get()->lookup(name);
    }

    return nullptr;
}