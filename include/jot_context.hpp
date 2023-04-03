#pragma once

#include "jot_alias_table.hpp"
#include "jot_ast.hpp"
#include "jot_diagnostics.hpp"
#include "jot_options.hpp"
#include "jot_source_manager.hpp"
#include "jot_type.hpp"

#include <memory>
#include <unordered_map>

enum FunctionDeclarationKind {
    NORMAL_FUNCTION,
    PREFIX_FUNCTION,
    INFIX_FUNCTION,
    POSTFIX_FUNCTION,
};

struct JotContext {
    JotContext() : diagnostics(JotDiagnosticEngine(source_manager)) {}

    JotOptions options;
    JotDiagnosticEngine diagnostics;
    JotSourceManager source_manager;
    JotAliasTable type_alias_table;

    // Declarations Informations
    std::unordered_map<std::string, FunctionDeclarationKind> functions;
    std::unordered_map<std::string, std::shared_ptr<JotStructType>> structures;
    std::unordered_map<std::string, std::shared_ptr<JotEnumType>> enumerations;
    std::unordered_map<std::string, Shared<Expression>> constants_table;
};