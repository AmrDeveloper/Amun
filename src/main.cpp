#include "../include/jot_basic.hpp"
#include "../include/jot_command.hpp"
#include "../include/jot_compiler.hpp"
#include "../include/jot_context.hpp"
#include "../include/jot_files.hpp"

#include <memory>

#define unused [[maybe_unused]]
#define JOT_VERSION "0.0.1"

auto execute_create_command(unused int argc, char** argv) -> int
{
    if (argc < 3) {
        printf("Invalid number of arguments for create command expect %i but got %i\n", 3, argc);
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
    string_stream << "import \"cstdio\"" << '\n';
    string_stream << "\n";
    string_stream << "fun main() int32 {" << '\n';
    string_stream << R"(    puts("Hello, World!\n");)" << '\n';
    string_stream << "    return 0;" << '\n';
    string_stream << "}";

    create_file_with_content(project_name + "/main.jot", string_stream.str());

    printf("New Project %s is created\n", project_name.c_str());

    return EXIT_SUCCESS;
}

auto execute_compile_command(unused int argc, char** argv) -> int
{
    if (argc < 3) {
        printf("Invalid number of arguments for `compile` command expect at last %i but got %i\n",
               3, argc);
        printf("Usage : %s compile <file> <options>\n", argv[0]);
        return EXIT_FAILURE;
    }

    const char* source_file = argv[2];
    if (!is_file_exists(source_file)) {
        printf("Path %s not exists\n", source_file);
        return EXIT_FAILURE;
    }

    if (!is_ends_with(source_file, ".jot")) {
        printf("Invalid source file extension, valid source file must end with `.jot`\n");
        return EXIT_FAILURE;
    }

    auto jot_context = std::make_shared<JotContext>();
    parse_compiler_options(&jot_context->options, argc, argv);

    JotCompiler jot_compiler(jot_context);
    return jot_compiler.compile_source_code(source_file);
}

auto execute_check_command(unused int argc, char** argv) -> int
{
    if (argc < 3) {
        printf("Invalid number of arguments for `check` command expect %i but got %i\n", 3, argc);
        printf("Usage : %s check <file>\n", argv[0]);
        return EXIT_FAILURE;
    }

    const char* source_file = argv[2];
    if (!is_file_exists(source_file)) {
        printf("Path %s not exists\n", source_file);
        return EXIT_FAILURE;
    }

    if (!is_ends_with(source_file, ".jot")) {
        printf("Invalid source file extension, valid source file must end with `.jot`\n");
        return EXIT_FAILURE;
    }

    auto jot_context = std::make_shared<JotContext>();
    JotCompiler jot_compiler(jot_context);
    return jot_compiler.check_source_code(source_file);
}

auto execute_version_command(unused int argc, unused char** argv) -> int
{
    printf("Jot version is %s\n", JOT_VERSION);
    return EXIT_SUCCESS;
}

auto execute_help_command(unused int argc, char** argv) -> int
{
    printf("Usage: %s <command> <options>\n", argv[0]);
    printf("Commands:\n");
    printf("    - create  <name>           : Create a new project with Hello world code.\n");
    printf("    - compile <file> <options> : Compile source file with options.\n");
    printf("    - check   <file>           : Check if the source code is valid.\n");
    printf("    - version                  : Print the current Jot version.\n");
    printf("    - help                     : Print how to use and list of commands.\n");
    printf("Options:\n");
    printf("    -o  <name>                 : Set the output exeutable name.\n");
    printf("    -w                         : Enable reporting warns, disabled by default.\n");
    printf("    -werr                      : Convert warns to erros.\n");
    return EXIT_SUCCESS;
}

auto main(int argc, char** argv) -> int
{
    JotCommands jot_commands;
    jot_commands.registerCommand("create", execute_create_command);
    jot_commands.registerCommand("compile", execute_compile_command);
    jot_commands.registerCommand("check", execute_check_command);
    jot_commands.registerCommand("version", execute_version_command);
    jot_commands.registerCommand("help", execute_help_command);
    return jot_commands.executeCommand(argc, argv);
}
