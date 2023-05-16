#pragma once

#include "../include/amun_ast.hpp"
#include "../include/amun_context.hpp"

#include <memory>
#include <unordered_set>

namespace amun {

class Compiler {
  public:
    explicit Compiler(Shared<amun::Context> context) : context(std::move(context)) {}

    auto compile_source_code(const char* source_file) -> int;

    auto compile_to_object_file_from_source_code(const char* source_file) -> int;

    auto emit_llvm_ir_from_source_code(const char* source_file) -> int;

    auto check_source_code(const char* source_file) -> int;

    auto parse_source_code(const char* source_file) -> Shared<CompilationUnit>;

  private:
    Shared<amun::Context> context;
};

} // namespace amun