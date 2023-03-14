#pragma once

#include "../include/jot_ast.hpp"
#include "../include/jot_context.hpp"

#include <memory>
#include <unordered_set>

class JotCompiler {
  public:
    explicit JotCompiler(Shared<JotContext> jot_context) : jot_context(std::move(jot_context)) {}

    auto compile_source_code(const char* source_file) -> int;

    auto check_source_code(const char* source_file) -> int;

    auto parse_source_code(const char* source_file) -> Shared<CompilationUnit>;

  private:
    Shared<JotContext> jot_context;
};