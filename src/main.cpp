#include "../include/jot_command.hpp"
#include "../include/jot_context.hpp"

int execute_compile_command([[maybe_unused]] int argc, [[maybe_unused]] char **argv) {
    const char *source = argv[2];
    JotContext jot_context;
    return jot_context.compile_source_code(source);
}

int execute_check_command([[maybe_unused]] int argc, [[maybe_unused]] char **argv) {
    const char *source = argv[2];
    JotContext jot_context;
    return jot_context.check_source_code(source);
}

int execute_version_command([[maybe_unused]] int argc, [[maybe_unused]] char **argv) {
    printf("Jot version is 0.0.1\n");
    return EXIT_SUCCESS;
}

int execute_help_command([[maybe_unused]] int argc, [[maybe_unused]] char **argv) {
    printf("Usage: %s <command> <options>\n", argv[0]);
    printf("Commands:\n");
    printf("    - compile <file> <options> : Compile source file with options.\n");
    printf("    - check   <file>           : Check if the source code is valid.\n");
    printf("    - version                  : Print the current Jot version.\n");
    printf("    - help                     : Print how to use and list of commands.\n");
    return EXIT_SUCCESS;
}

int main(int argc, char **argv) {
    JotCommands jot_commands;
    jot_commands.registerCommand("compile", execute_compile_command);
    jot_commands.registerCommand("check", execute_check_command);
    jot_commands.registerCommand("version", execute_version_command);
    jot_commands.registerCommand("help", execute_help_command);
    return jot_commands.executeCommand(argc, argv);
}
