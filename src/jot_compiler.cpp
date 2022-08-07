#include "../include/jot_compiler.hpp"
#include "../include/jot_files.hpp"
#include "../include/jot_llvm_backend.hpp"
#include "../include/jot_logger.hpp"
#include "../include/jot_parser.hpp"
#include "../include/jot_typechecker.hpp"

int JotCompiler::compile_source_code(const char *source_file) {
    auto compilation_unit = parse_source_code(source_file);

    JotTypeChecker type_checker;
    type_checker.check_compilation_unit(compilation_unit);

    JotLLVMBackend llvm_backend;
    auto llvm_ir_module = llvm_backend.compile(source_file, compilation_unit);

    const char *ouputFileName = "output.ll";
    std::error_code error_code;
    llvm::raw_fd_ostream output_stream(ouputFileName, error_code);
    if (error_code.message() != "Success") {
        return EXIT_FAILURE;
    }

    llvm_ir_module->print(output_stream, nullptr);

    return EXIT_SUCCESS;
}

int JotCompiler::check_source_code(const char *source_file) {
    auto compilation_unit = parse_source_code(source_file);

    JotTypeChecker type_checker;
    type_checker.check_compilation_unit(compilation_unit);

    return EXIT_SUCCESS;
}

std::shared_ptr<CompilationUnit> JotCompiler::parse_source_code(const char *source_file) {
    if (not is_file_exists(source_file)) {
        jot::loge << "Path " << source_file << " not exists\n";
        exit(EXIT_FAILURE);
    }
    auto source_content = read_file_content(source_file);
    auto tokenizer = std::make_unique<JotTokenizer>(source_file, source_content);
    JotParser parser(jot_context, std::move(tokenizer));
    return parser.parse_compilation_unit();
}
