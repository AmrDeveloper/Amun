#pragma once

#include "../include/jot_ast.hpp"
#include "../include/jot_context.hpp"

#include <memory>
#include <unordered_set>

class JotCompiler {
  public:
    JotCompiler(std::shared_ptr<JotContext> jot_context) : jot_context(jot_context) {}

    int compile_source_code(const char *source_file);

    int check_source_code(const char *source_file);

    std::shared_ptr<CompilationUnit> parse_source_code(const char *source_file);

  private:
    std::shared_ptr<JotContext> jot_context;

    // This variable will be refactored later to be a part flag of JotOptions
    bool should_report_warings = true;
};