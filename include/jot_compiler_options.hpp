#pragma once

#include <string>
#include <vector>

#define EXECUTABLE_NAME_FLAG "-o"
#define WARNINGS_FLAG "-w"
#define WARNS_TO_ERRORS_FLAG "-werr"
#define LINKER_EXTREA_FLAG "-l"

// Number of options that can modifed from Compiler CLI
#define NUMBER_OF_COMPILER_OPTIONS 4

// Set of configurations for the Jot Compiler
struct JotOptions {
    const char* executable_name = "output.o";
    const char* llvm_ir_file_name = "output.ll";

    bool should_report_warns = false;
    bool convert_warns_to_errors = false;

    std::vector<std::string> linker_extra_flags;
};

// Parse compiler options argv[3:],
// Exit the compiler with state Failure and error message if there are invalid options
auto parse_compiler_options(JotOptions* options, int argc, char** argv) -> void;

// Report error and exit if any compiler option is passed twice
auto check_passed_twice_option(const bool received_options[], int index, char* arg) -> void;
