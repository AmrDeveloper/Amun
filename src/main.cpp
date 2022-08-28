#include "../include/jot_command.hpp"
#include "../include/jot_compiler.hpp"
#include "../include/jot_context.hpp"
#include "../include/jot_files.hpp"

#include <memory>

#define unused [[maybe_unused]]
#define JOT_VERSION "0.0.1"

int execute_create_command(unused int argc, char **argv) {
    if (argc < 3) {
        printf("Invalid number of arguments expect %i but got %i\n", 3, argc);
        printf("Usage : %s create <project name>\n", argv[0]);
        return EXIT_FAILURE;
    }

    std::string project_name = argv[2];
    if (is_file_exists(project_name)) {
        printf("Directory with name %s is already exists.\n", project_name.c_str());
        return EXIT_FAILURE;
    }

    create_new_directory(project_name);

    std::stringstream string_stream;
    string_stream << "extern fun puts(c *char) int32;" << '\n';
    string_stream << "\n";
    string_stream << "fun main() int32 {" << '\n';
    string_stream << R"(    puts("Hello, World!\n");)" << '\n';
    string_stream << "    return 0;" << '\n';
    string_stream << "}";

    create_file_with_content(project_name + "/main.jot", string_stream.str());

    printf("New Project %s is created\n", project_name.c_str());

    return EXIT_SUCCESS;
}

int execute_compile_command(unused int argc, char **argv) {
    const char *source = argv[2];
    auto jot_context = std::make_shared<JotContext>();
    JotCompiler jot_compiler(jot_context);
    return jot_compiler.compile_source_code(source);
}

int execute_check_command(unused int argc, char **argv) {
    const char *source = argv[2];
    auto jot_context = std::make_shared<JotContext>();
    JotCompiler jot_compiler(jot_context);
    return jot_compiler.check_source_code(source);
}

int execute_version_command(unused int argc, unused char **argv) {
    printf("Jot version is %s\n", JOT_VERSION);
    return EXIT_SUCCESS;
}

int execute_help_command(unused int argc, char **argv) {
    printf("Usage: %s <command> <options>\n", argv[0]);
    printf("Commands:\n");
    printf("    - create  <name>           : Create a new project with Hello world code.\n");
    printf("    - compile <file> <options> : Compile source file with options.\n");
    printf("    - check   <file>           : Check if the source code is valid.\n");
    printf("    - version                  : Print the current Jot version.\n");
    printf("    - help                     : Print how to use and list of commands.\n");
    return EXIT_SUCCESS;
}

int main(int argc, char **argv) {
    JotCommands jot_commands;
    jot_commands.registerCommand("create", execute_create_command);
    jot_commands.registerCommand("compile", execute_compile_command);
    jot_commands.registerCommand("check", execute_check_command);
    jot_commands.registerCommand("version", execute_version_command);
    jot_commands.registerCommand("help", execute_help_command);
    return jot_commands.executeCommand(argc, argv);
}
