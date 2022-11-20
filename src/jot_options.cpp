#include "../include/jot_options.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>

void parse_compiler_options(JotOptions* options, int argc, char** argv)
{
    int start_index = 3;
    for (int i = start_index; i < argc; i++) {
        auto argument = argv[i];

        // Change the executable name
        if (strcmp(argument, EXECUTABLE_NAME_FLAG) == 0) {
            if (i == argc - 1) {
                printf("Flag -o expect executable name after it, but found no thing\n");
                std::exit(EXIT_FAILURE);
            }

            i = i + 1;

            // TODO: check that the new name is valid
            options->executable_name = argv[i];
            continue;
        }

        // Enable reporting warns
        if (strcmp(argument, WARNINGS_FLAG) == 0) {
            options->should_report_warns = true;
            continue;
        }

        // Convert warns to errors
        if (strcmp(argument, WARNS_TO_ERRORS_FLAG) == 0) {
            options->convert_warns_to_errors = true;
            options->should_report_warns = true;
            continue;
        }

        printf("Unkown compiler flag with name `%s`\n", argument);
        printf("Please run `%s help` to see all available options\n", argv[0]);
        std::exit(EXIT_FAILURE);
    }
}
