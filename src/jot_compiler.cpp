#include "../include/jot_compiler.hpp"
#include "../include/jot_external_linker.hpp"
#include "../include/jot_files.hpp"
#include "../include/jot_llvm_backend.hpp"
#include "../include/jot_logger.hpp"
#include "../include/jot_parser.hpp"
#include "../include/jot_typechecker.hpp"

#include <llvm/ADT/Triple.h>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/MC/TargetRegistry.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/Host.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Target/TargetMachine.h>
#include <llvm/Target/TargetOptions.h>

auto JotCompiler::compile_source_code(const char* source_file) -> int
{
    auto external_linker = JotExternalLinker();

    // Check aviable linkers before start compiling
    if (!external_linker.check_aviable_linker()) {
        std::cout << "No aviable linker please install one of thoses options\n";
        for (const auto& linker_name : external_linker.potentials_linkes_names) {
            std::cout << "    -" << linker_name << "\n";
        }
        exit(EXIT_FAILURE);
    }

    auto compilation_unit = parse_source_code(source_file);

    JotTypeChecker type_checker(jot_context);
    type_checker.check_compilation_unit(compilation_unit);

    if (jot_context->options.should_report_warns and
        jot_context->diagnostics.level_count(DiagnosticLevel::WARNING) > 0) {
        jot_context->diagnostics.report_diagnostics(DiagnosticLevel::WARNING);
    }

    if (jot_context->diagnostics.level_count(DiagnosticLevel::ERROR) > 0) {
        jot_context->diagnostics.report_diagnostics(DiagnosticLevel::ERROR);
        return EXIT_FAILURE;
    }

    if (jot_context->options.convert_warns_to_errors and
        jot_context->diagnostics.level_count(DiagnosticLevel::WARNING) > 0) {
        return EXIT_FAILURE;
    }

    JotLLVMBackend llvm_backend;
    auto llvm_ir_module = llvm_backend.compile(source_file, compilation_unit);

    // Initalize native targers
    llvm::InitializeNativeTarget();
    llvm::InitializeNativeTargetAsmParser();
    llvm::InitializeNativeTargetAsmPrinter();

    auto object_file_path = jot_context->options.executable_name;

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

    return result;
}

auto JotCompiler::emit_llvm_ir_from_source_code(const char* source_file) -> int
{
    auto compilation_unit = parse_source_code(source_file);

    JotTypeChecker type_checker(jot_context);
    type_checker.check_compilation_unit(compilation_unit);

    if (jot_context->options.should_report_warns and
        jot_context->diagnostics.level_count(DiagnosticLevel::WARNING) > 0) {
        jot_context->diagnostics.report_diagnostics(DiagnosticLevel::WARNING);
    }

    if (jot_context->diagnostics.level_count(DiagnosticLevel::ERROR) > 0) {
        jot_context->diagnostics.report_diagnostics(DiagnosticLevel::ERROR);
        exit(EXIT_FAILURE);
    }

    if (jot_context->options.convert_warns_to_errors and
        jot_context->diagnostics.level_count(DiagnosticLevel::WARNING) > 0) {
        exit(EXIT_FAILURE);
    }

    JotLLVMBackend llvm_backend;
    auto llvm_ir_module = llvm_backend.compile(source_file, compilation_unit);

    std::error_code error_code;
    llvm::raw_fd_ostream output_stream(jot_context->options.llvm_ir_file_name, error_code);
    if (error_code.message() != "Success") {
        std::cout << "Can't create output file " << error_code.message() << '\n';
        return EXIT_FAILURE;
    }

    llvm_ir_module->print(output_stream, nullptr);
    std::cout << "Successfully compiled to " << jot_context->options.llvm_ir_file_name << '\n';
    return EXIT_SUCCESS;
}

auto JotCompiler::check_source_code(const char* source_file) -> int
{
    auto compilation_unit = parse_source_code(source_file);

    JotTypeChecker type_checker(jot_context);
    type_checker.check_compilation_unit(compilation_unit);

    if (jot_context->options.should_report_warns and
        jot_context->diagnostics.level_count(DiagnosticLevel::WARNING) > 0) {
        jot_context->diagnostics.report_diagnostics(DiagnosticLevel::WARNING);
    }

    if (jot_context->diagnostics.level_count(DiagnosticLevel::ERROR) > 0) {
        jot_context->diagnostics.report_diagnostics(DiagnosticLevel::ERROR);
        exit(EXIT_FAILURE);
    }

    if (jot_context->options.convert_warns_to_errors and
        jot_context->diagnostics.level_count(DiagnosticLevel::WARNING) > 0) {
        exit(EXIT_FAILURE);
    }

    std::cout << "Source code in " << source_file << " is valid" << std::endl;
    return EXIT_SUCCESS;
}

auto JotCompiler::parse_source_code(const char* source_file) -> std::shared_ptr<CompilationUnit>
{
    if (not is_file_exists(source_file)) {
        jot::loge << "Path " << source_file << " not exists\n";
        exit(EXIT_FAILURE);
    }

    auto file_id = jot_context->source_manager.register_source_path(source_file);
    auto source_content = read_file_content(source_file);
    JotTokenizer tokenizer(file_id, source_content);
    JotParser parser(jot_context, tokenizer);

    auto compilation_unit = parser.parse_compilation_unit();

    if (jot_context->diagnostics.level_count(DiagnosticLevel::ERROR) > 0) {
        jot_context->diagnostics.report_diagnostics(DiagnosticLevel::ERROR);
        exit(EXIT_FAILURE);
    }

    return compilation_unit;
}
