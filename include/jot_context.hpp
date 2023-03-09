#pragma once

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

    JotOptions          options;
    JotDiagnosticEngine diagnostics;
    JotSourceManager    source_manager;

    // Declarations Informations
    std::unordered_map<std::string, FunctionDeclarationKind>        functions;
    std::unordered_map<std::string, std::shared_ptr<JotStructType>> structures;
    std::unordered_map<std::string, std::shared_ptr<JotEnumType>>   enumerations;
};