#pragma once

#include <string>
#include <vector>

#include <llvm/Support/Program.h>

namespace amun {

struct ExternalLinker {
    auto link(std::string object_file_path) -> int;
    auto check_aviable_linker() -> bool;

    std::vector<std::string> potentials_linkes_names = {"clang", "gcc"};
    std::vector<std::string> linker_flags = {"-no-pie", "-flto"};
    std::string current_linker_name = potentials_linkes_names[0];
};

} // namespace amun