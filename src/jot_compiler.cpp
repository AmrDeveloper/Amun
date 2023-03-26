#include "../include/jot_compiler.hpp"
#include "../include/jot_files.hpp"
#include "../include/jot_llvm_backend.hpp"
#include "../include/jot_logger.hpp"
#include "../include/jot_parser.hpp"
#include "../include/jot_typechecker.hpp"

auto JotCompiler::compile_source_code(const char* source_file) -> int
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
    auto           llvm_ir_module = llvm_backend.compile(source_file, compilation_unit);

    std::error_code      error_code;
    llvm::raw_fd_ostream output_stream(jot_context->options.executable_name, error_code);
    if (error_code.message() != "Success") {
        std::cout << "Can't create output file " << error_code.message() << '\n';
        return EXIT_FAILURE;
    }

    llvm_ir_module->print(output_stream, nullptr);
    std::cout << "File " << source_file << " Successfully compiled to "
              << jot_context->options.executable_name << '\n';
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

    auto         file_id = jot_context->source_manager.register_source_path(source_file);
    auto         source_content = read_file_content(source_file);
    JotTokenizer tokenizer(file_id, source_content);
    JotParser    parser(jot_context, tokenizer);

    auto compilation_unit = parser.parse_compilation_unit();

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

    return compilation_unit;
}
