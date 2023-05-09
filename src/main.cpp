#include "../include/amun_basic.hpp"
#include "../include/amun_command.hpp"
#include "../include/amun_compiler.hpp"
#include "../include/amun_context.hpp"
#include "../include/amun_files.hpp"

#include <memory>

#define unused [[maybe_unused]]

auto execute_create_command(unused int argc, char** argv) -> int
{
    if (argc < 3) {
        printf("Invalid number of arguments for create command expect %i but got %i\n", 3, argc);
        printf("Usage : %s create <project name>\n", argv[0]);
        return EXIT_FAILURE;
    }

    std::string project_name = argv[2];
    if (amun::is_file_exists(project_name)) {
        printf("Directory with name %s is already exists.\n", project_name.c_str());
        return EXIT_FAILURE;
    }

    amun::create_new_directory(project_name);

    std::stringstream string_stream;
    string_stream << "import \"cstdio\"" << '\n';
    string_stream << "\n";
    string_stream << "fun main() {" << '\n';
    string_stream << R"(    printf("Hello, World!\n");)" << '\n';
    string_stream << "}";

    amun::create_file_with_content(project_name + "/main.amun", string_stream.str());

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
    if (!amun::is_file_exists(source_file)) {
        printf("Path %s not exists\n", source_file);
        return EXIT_FAILURE;
    }

    if (!is_ends_with(source_file, AMUN_LANGUAGE_EXTENSION)) {
        printf("Invalid source file extension, file must end with `%s`\n", AMUN_LANGUAGE_EXTENSION);
        return EXIT_FAILURE;
    }

    auto context = std::make_shared<amun::Context>();
    parse_compiler_options(&context->options, argc, argv);

    amun::Compiler compiler(context);
    return compiler.compile_source_code(source_file);
}

auto emit_llvm_ir_coomand(unused int argc, char** argv) -> int
{
    if (argc < 3) {
        printf("Invalid number of arguments for `compile` command expect at last %i but got %i\n",
               3, argc);
        printf("Usage : %s compile <file> <options>\n", argv[0]);
        return EXIT_FAILURE;
    }

    const char* source_file = argv[2];
    if (!amun::is_file_exists(source_file)) {
        printf("Path %s not exists\n", source_file);
        return EXIT_FAILURE;
    }

    if (!is_ends_with(source_file, AMUN_LANGUAGE_EXTENSION)) {
        printf("Invalid source file extension, file must end with `%s`\n", AMUN_LANGUAGE_EXTENSION);
        return EXIT_FAILURE;
    }

    auto context = std::make_shared<amun::Context>();
    parse_compiler_options(&context->options, argc, argv);

    amun::Compiler compiler(context);
    return compiler.emit_llvm_ir_from_source_code(source_file);
}

auto execute_check_command(unused int argc, char** argv) -> int
{
    if (argc < 3) {
        printf("Invalid number of arguments for `check` command expect %i but got %i\n", 3, argc);
        printf("Usage : %s check <file>\n", argv[0]);
        return EXIT_FAILURE;
    }

    const char* source_file = argv[2];
    if (!amun::is_file_exists(source_file)) {
        printf("Path %s not exists\n", source_file);
        return EXIT_FAILURE;
    }

    if (!is_ends_with(source_file, AMUN_LANGUAGE_EXTENSION)) {
        printf("Invalid source file extension, file must end with `%s`\n", AMUN_LANGUAGE_EXTENSION);
        return EXIT_FAILURE;
    }

    auto context = std::make_shared<amun::Context>();
    amun::Compiler compiler(context);
    return compiler.check_source_code(source_file);
}

auto execute_version_command(unused int argc, unused char** argv) -> int
{
    printf("Language version is %s\n", AMUN_LANGUAGE_VERSION);
    return EXIT_SUCCESS;
}

auto execute_help_command(unused int argc, char** argv) -> int
{
    printf("Usage: %s <command> <options>\n", argv[0]);
    printf("Commands:\n");
    printf("    - create  <name>           : Create a new project with Hello world code.\n");
    printf("    - compile <file> <options> : Compile source files to executables with options.\n");
    printf("    - emit-ir <file> <options> : Compile source to llvm ir files with options.\n");
    printf("    - check   <file>           : Check if the source code is valid.\n");
    printf("    - version                  : Print the current compiler version.\n");
    printf("    - help                     : Print how to use and list of commands.\n");
    printf("Options:\n");
    printf("    -o  <name>                 : Set the output exeutable name.\n");
    printf("    -w                         : Enable reporting warns, disabled by default.\n");
    printf("    -werr                      : Convert warns to erros.\n");
    return EXIT_SUCCESS;
}

auto main(int argc, char** argv) -> int
{
    amun::CommandMap command_map;
    command_map.registerCommand("create", execute_create_command);
    command_map.registerCommand("compile", execute_compile_command);
    command_map.registerCommand("emit-ir", emit_llvm_ir_coomand);
    command_map.registerCommand("check", execute_check_command);
    command_map.registerCommand("version", execute_version_command);
    command_map.registerCommand("help", execute_help_command);
    return command_map.executeCommand(argc, argv);
}
