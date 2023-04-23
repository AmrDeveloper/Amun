#include <sstream>

#include "../include/amun_external_linker.hpp"

auto amun::ExternalLinker::link(std::string object_file_path) -> int
{
    std::stringstream linker_command_builder;

    // Append linker name
    linker_command_builder << current_linker_name;

    // Append linker flags
    for (const auto& linker_flag : linker_flags) {
        linker_command_builder << " " + linker_flag;
    }

    // Append object field path
    linker_command_builder << " " + object_file_path;

    // Set name for executable file to be the same name of oject file but removing .o extension
    linker_command_builder << " -o ";
    linker_command_builder << object_file_path.erase(object_file_path.size() - 2);

    // Build the full linker command
    const std::string linker_coomand = linker_command_builder.str();

    // Execute the command
    return std::system(linker_coomand.c_str());
}

auto amun::ExternalLinker::check_aviable_linker() -> bool
{
    for (const auto& linker_name : potentials_linkes_names) {
        auto optional_path = llvm::sys::findProgramByName(linker_name);
        if (auto error_code = optional_path.getError()) {
            continue;
        }
        current_linker_name = linker_name;
        return true;
    }

    return false;
}