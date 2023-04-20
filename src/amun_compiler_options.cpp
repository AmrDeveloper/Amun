#include "../include/amun_compiler_options.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>

auto amun::parse_compiler_options(amun::CompilerOptions* options, int argc, char** argv) -> void
{
    bool received_options[NUMBER_OF_COMPILER_OPTIONS] = {false};

    int start_index = 3;
    for (int i = start_index; i < argc; i++) {
        auto argument = argv[i];

        // Change the executable name
        if (strcmp(argument, EXECUTABLE_NAME_FLAG) == 0) {
            amun::check_passed_twice_option(received_options, 0, argument);

            if (i == argc - 1) {
                printf("Flag `%s` expect executable name after it, but found no thing\n", argument);
                exit(EXIT_FAILURE);
            }

            i += 1;

            // TODO: check that the new name is valid
            options->executable_name = argv[i];
            received_options[0] = true;
            continue;
        }

        // Enable reporting warns
        if (strcmp(argument, WARNINGS_FLAG) == 0) {
            amun::check_passed_twice_option(received_options, 1, argument);
            options->should_report_warns = true;
            received_options[1] = true;
            continue;
        }

        // Convert warns to errors
        if (strcmp(argument, WARNS_TO_ERRORS_FLAG) == 0) {
            amun::check_passed_twice_option(received_options, 2, argument);
            options->convert_warns_to_errors = true;
            options->should_report_warns = true;
            received_options[2] = true;
            continue;
        }

        // Accept extra arguments for the external or internal linker
        if (strcmp(argument, LINKER_EXTREA_FLAG) == 0) {
            amun::check_passed_twice_option(received_options, 3, argument);
            for (int e = i + 1; e < argc; e++) {
                options->linker_extra_flags.push_back(argv[e]);
            }
            received_options[3] = true;
            return;
        }

        printf("ERROR: Unkown compiler flag with name `%s`\n", argument);
        printf("Please run `%s help` to see all available options\n", argv[0]);
        exit(EXIT_FAILURE);
    }
}

auto amun::check_passed_twice_option(const bool received_options[], int index, char* arg) -> void
{
    if (received_options[index]) {
        printf("ERROR: Flag `%s` is passed twice\n", arg);
        exit(EXIT_FAILURE);
    }
}