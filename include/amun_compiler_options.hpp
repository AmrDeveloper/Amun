#pragma once

#include <string>
#include <vector>

#define EXECUTABLE_NAME_FLAG "-o"
#define WARNINGS_FLAG "-w"
#define WARNS_TO_ERRORS_FLAG "-werr"
#define LINKER_EXTREA_FLAG "-l"

// Number of options that can modifed from Compiler CLI
#define NUMBER_OF_COMPILER_OPTIONS 4

namespace amun {

// Set of configurations for the Compiler
struct CompilerOptions {
    std::string output_file_name = "output";

    bool should_report_warns = false;
    bool convert_warns_to_errors = false;

    bool use_cpu_features = true;

    std::vector<std::string> linker_extra_flags;
};

// Parse compiler options argv[3:],
// Exit the compiler with state Failure and error message if there are invalid options
auto parse_compiler_options(CompilerOptions* options, int argc, char** argv) -> void;

// Report error and exit if any compiler option is passed twice
auto check_passed_twice_option(const bool received_options[], int index, char* arg) -> void;

} // namespace amun