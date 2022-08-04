#include "../include/jot_command.hpp"
#include "../include/jot_files.hpp"
#include "../include/jot_llvm_backend.hpp"
#include "../include/jot_logger.hpp"
#include "../include/jot_parser.hpp"
#include "../include/jot_tokenizer.hpp"
#include "../include/jot_typechecker.hpp"

#include <memory>

int execute_compile_command([[maybe_unused]] int argc, [[maybe_unused]] char **argv) {
    const char *source = argv[2];
    auto source_content = read_file_content(source);

    auto tokenizer = std::make_unique<JotTokenizer>(source, source_content);

    JotParser parser(std::move(tokenizer));
    auto compilation_unit = parser.parse_compilation_unit();

    JotTypeChecker type_checker;
    type_checker.check_compilation_unit(compilation_unit);

    JotLLVMBackend llvm_backend;
    auto llvm_ir_module = llvm_backend.compile(source, compilation_unit);

    llvm_ir_module->print(llvm::errs(), nullptr);
    return EXIT_SUCCESS;
}

int execute_check_command([[maybe_unused]] int argc, [[maybe_unused]] char **argv) {
    const char *source = argv[2];
    auto source_content = read_file_content(source);

    auto tokenizer = std::make_unique<JotTokenizer>(source, source_content);

    JotParser parser(std::move(tokenizer));
    auto compilation_unit = parser.parse_compilation_unit();

    JotTypeChecker type_checker;
    type_checker.check_compilation_unit(compilation_unit);
    return EXIT_SUCCESS;
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
    jot_commands.registerCommand("version", execute_version_command);
    jot_commands.registerCommand("help", execute_help_command);
    return jot_commands.executeCommand(argc, argv);
}
