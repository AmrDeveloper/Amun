#pragma once

#include "../include/jot_files.hpp"
#include "../include/jot_llvm_backend.hpp"
#include "../include/jot_parser.hpp"
#include "../include/jot_typechecker.hpp"

#include <memory>
#include <unordered_set>

class JotContext {
  public:
    int compile_source_code(const char *source_file);

    int check_source_code(const char *source_file);

    std::shared_ptr<CompilationUnit> parse_source_code(const char *source_file);

  private:
    std::unordered_set<std::string> visited_files;
};