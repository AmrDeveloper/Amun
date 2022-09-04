#pragma once

#define EXECUTABLE_NAME_FLAG "-o"
#define WARNINGS_FLAG "-w"
#define WARNS_TO_ERRORS_FLAG "-werr"

// Set of configurations for the Jot Compiler
struct JotOptions {
    const char *executable_name = "output.ll";
    bool should_report_warns = false;
    bool convert_warns_to_errors = false;
};

// Parse compiler options argv[3:],
// Exit the compiler with state Failure and error message if there are invalid options
void parse_compiler_options(JotOptions *options, int argc, char **argv);
