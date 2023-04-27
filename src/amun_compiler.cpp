#include "../include/amun_compiler.hpp"
#include "../include/amun_external_linker.hpp"
#include "../include/amun_files.hpp"
#include "../include/amun_llvm_backend.hpp"
#include "../include/amun_logger.hpp"
#include "../include/amun_parser.hpp"
#include "../include/amun_typechecker.hpp"

#include <llvm/ADT/Triple.h>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/MC/TargetRegistry.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/Host.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Target/TargetMachine.h>
#include <llvm/Target/TargetOptions.h>

auto amun::Compiler::compile_source_code(const char* source_file) -> int
{
    auto external_linker = amun::ExternalLinker();

    // Append extra linker flags passed to the amun compiler
    if (!context->options.linker_extra_flags.empty()) {
        for (const auto& option_flag : context->options.linker_extra_flags) {
            external_linker.linker_flags.push_back(option_flag);
        }
    }

    // Check aviable linkers before start compiling
    if (!external_linker.check_aviable_linker()) {
        std::cout << "No aviable linker please install one of thoses options\n";
        for (const auto& linker_name : external_linker.potentials_linkes_names) {
            std::cout << "    -" << linker_name << "\n";
        }
        exit(EXIT_FAILURE);
    }

    auto compilation_unit = parse_source_code(source_file);

    amun::TypeChecker type_checker(context);
    type_checker.check_compilation_unit(compilation_unit);

    if (context->options.should_report_warns and
        context->diagnostics.level_count(amun::DiagnosticLevel::WARNING) > 0) {
        context->diagnostics.report_diagnostics(amun::DiagnosticLevel::WARNING);
    }

    if (context->diagnostics.level_count(amun::DiagnosticLevel::ERROR) > 0) {
        context->diagnostics.report_diagnostics(amun::DiagnosticLevel::ERROR);
        return EXIT_FAILURE;
    }

    if (context->options.convert_warns_to_errors and
        context->diagnostics.level_count(amun::DiagnosticLevel::WARNING) > 0) {
        return EXIT_FAILURE;
    }

    amun::LLVMBackend llvm_backend;
    auto llvm_ir_module = llvm_backend.compile(source_file, compilation_unit);

    // Initalize native targers
    llvm::InitializeNativeTarget();
    llvm::InitializeNativeTargetAsmParser();
    llvm::InitializeNativeTargetAsmPrinter();

    std::string object_file_path = context->options.output_file_name + ".o";

    std::error_code object_file_error;
    auto flags = llvm::sys::fs::OF_None;
    llvm::raw_fd_ostream stream(object_file_path, object_file_error, flags);
    if (object_file_error.message() != "Success") {
        std::cout << "Can't create output file " << object_file_error.message() << '\n';
        return EXIT_FAILURE;
    }

    // Get current machine target triple
    auto target_triple = llvm::sys::getDefaultTargetTriple();

    // Check if this target is available
    std::string lookup_target_error;
    auto target = llvm::TargetRegistry::lookupTarget(target_triple, lookup_target_error);
    if (!lookup_target_error.empty()) {
        std::cout << lookup_target_error << '\n';
        return EXIT_FAILURE;
    }

    llvm::TargetOptions opt;
    auto rm = llvm::Optional<llvm::Reloc::Model>();
    llvm::legacy::PassManager pass_manager;

    constexpr auto CPU = "generic";
    constexpr auto FEATURES = "";
    auto target_machine = target->createTargetMachine(target_triple, CPU, FEATURES, opt, rm);

    auto file_type = llvm::CGFT_ObjectFile;
    if (target_machine->addPassesToEmitFile(pass_manager, stream, nullptr, file_type, true)) {
        std::cout << "Target machine can't emit a file of this type" << '\n';
        return EXIT_FAILURE;
    }

    pass_manager.run(*llvm_ir_module);
    stream.flush();

    // Link object file with optional libraries into executable
    auto result = external_linker.link(object_file_path);
    if (result == 0) {
        std::cout << "Successfully compiled " << source_file << " to executable\n";
    }
    else {
        std::cout << "External Linker error\n";
    }
    return result;
}

auto amun::Compiler::emit_llvm_ir_from_source_code(const char* source_file) -> int
{
    auto compilation_unit = parse_source_code(source_file);

    amun::TypeChecker type_checker(context);
    type_checker.check_compilation_unit(compilation_unit);

    if (context->options.should_report_warns and
        context->diagnostics.level_count(amun::DiagnosticLevel::WARNING) > 0) {
        context->diagnostics.report_diagnostics(amun::DiagnosticLevel::WARNING);
    }

    if (context->diagnostics.level_count(amun::DiagnosticLevel::ERROR) > 0) {
        context->diagnostics.report_diagnostics(amun::DiagnosticLevel::ERROR);
        exit(EXIT_FAILURE);
    }

    if (context->options.convert_warns_to_errors and
        context->diagnostics.level_count(amun::DiagnosticLevel::WARNING) > 0) {
        exit(EXIT_FAILURE);
    }

    amun::LLVMBackend llvm_backend;
    auto llvm_ir_module = llvm_backend.compile(source_file, compilation_unit);

    std::string ir_file_name = context->options.output_file_name + ".ll";

    std::error_code error_code;
    llvm::raw_fd_ostream output_stream(ir_file_name, error_code);
    if (error_code.message() != "Success") {
        std::cout << "Can't create output file " << error_code.message() << '\n';
        return EXIT_FAILURE;
    }

    llvm_ir_module->print(output_stream, nullptr);
    std::cout << "Successfully compiled " << source_file << " to " << ir_file_name << '\n';
    return EXIT_SUCCESS;
}

auto amun::Compiler::check_source_code(const char* source_file) -> int
{
    auto compilation_unit = parse_source_code(source_file);

    amun::TypeChecker type_checker(context);
    type_checker.check_compilation_unit(compilation_unit);

    if (context->options.should_report_warns and
        context->diagnostics.level_count(amun::DiagnosticLevel::WARNING) > 0) {
        context->diagnostics.report_diagnostics(amun::DiagnosticLevel::WARNING);
    }

    if (context->diagnostics.level_count(amun::DiagnosticLevel::ERROR) > 0) {
        context->diagnostics.report_diagnostics(amun::DiagnosticLevel::ERROR);
        exit(EXIT_FAILURE);
    }

    if (context->options.convert_warns_to_errors and
        context->diagnostics.level_count(amun::DiagnosticLevel::WARNING) > 0) {
        exit(EXIT_FAILURE);
    }

    std::cout << "Source code in " << source_file << " is valid" << std::endl;
    return EXIT_SUCCESS;
}

auto amun::Compiler::parse_source_code(const char* source_file) -> std::shared_ptr<CompilationUnit>
{
    if (!amun::is_file_exists(source_file)) {
        amun::loge << "Path " << source_file << " not exists\n";
        exit(EXIT_FAILURE);
    }

    auto file_id = context->source_manager.register_source_path(source_file);
    auto source_content = read_file_content(source_file);
    amun::Tokenizer tokenizer(file_id, source_content);
    amun::Parser parser(context, tokenizer);

    auto compilation_unit = parser.parse_compilation_unit();

    if (context->diagnostics.level_count(amun::DiagnosticLevel::ERROR) > 0) {
        context->diagnostics.report_diagnostics(amun::DiagnosticLevel::ERROR);
        exit(EXIT_FAILURE);
    }

    return compilation_unit;
}
