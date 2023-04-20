#pragma once

#include "amun_alias_table.hpp"
#include "amun_ast.hpp"
#include "amun_compiler_options.hpp"
#include "amun_diagnostics.hpp"
#include "amun_source_manager.hpp"
#include "amun_type.hpp"

#include <memory>
#include <unordered_map>

namespace amun {

enum FunctionKind {
    NORMAL_FUNCTION,
    PREFIX_FUNCTION,
    INFIX_FUNCTION,
    POSTFIX_FUNCTION,
};

struct Context {
    Context() : diagnostics(amun::DiagnosticEngine(source_manager)) {}

    amun::CompilerOptions options;
    amun::DiagnosticEngine diagnostics;
    amun::SourceManager source_manager;
    amun::AliasTable type_alias_table;

    // Declarations Informations
    std::unordered_map<std::string, FunctionKind> functions;
    std::unordered_map<std::string, std::shared_ptr<amun::StructType>> structures;
    std::unordered_map<std::string, std::shared_ptr<amun::EnumType>> enumerations;
    std::unordered_map<std::string, Shared<Expression>> constants_table;
};

} // namespace amun