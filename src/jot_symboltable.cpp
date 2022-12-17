#include "../include/jot_symboltable.hpp"

bool JotSymbolTable::define(const std::string& name, std::any value)
{
    // Should only check in the current scope not all scopes
    if (environment.contains(name))
        return false;
    environment[name] = std::move(value);
    return true;
}

bool JotSymbolTable::is_defined(const std::string& name)
{
    if (environment.contains(name))
        return true;
    return parent_symbol_table && parent_symbol_table->is_defined(name);
}

bool JotSymbolTable::update(const std::string& name, std::any value)
{
    if (is_defined(name)) {
        environment[name] = value;
        return true;
    }
    return false;
}

std::any JotSymbolTable::lookup(const std::string& name)
{
    if (environment.contains(name)) {
        return environment[name];
    }

    if (parent_symbol_table) {
        return parent_symbol_table->lookup(name);
    }

    return nullptr;
}