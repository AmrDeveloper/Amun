#include "../include/jot_tokenizer.hpp"
#include "../include/jot_parser.hpp"
#include "../include/jot_typechecker.hpp"
#include "../include/jot_llvm_backend.hpp"
#include "../include/jot_logger.hpp"
#include "../include/jot_files.hpp"

#include <memory>
#include <chrono>

int main([[maybe_unused]] int argc, [[maybe_unused]] char** argv) {
    const char* file_name = "first.jot";
    const char* source = "../samples/first.jot";

    auto parsing_start_time = std::chrono::high_resolution_clock::now();

    auto source_content = read_file_content(source);
    auto tokenizer = std::make_unique<JotTokenizer>(file_name, source_content);
    JotParser parser(std::move(tokenizer));
    auto compilation_unit = parser.parse_compilation_unit();

    auto parsing_end_time = std::chrono::high_resolution_clock::now();
    auto parsing_duration = duration_cast<std::chrono::milliseconds>(parsing_end_time - parsing_start_time);

    auto checker_start_time = std::chrono::high_resolution_clock::now();

    JotTypeChecker type_checker;
    type_checker.check_compilation_unit(compilation_unit);

    auto checker_end_time = std::chrono::high_resolution_clock::now();
    auto checker_duration = duration_cast<std::chrono::milliseconds>(checker_end_time - checker_start_time);

    auto compiling_start_time = std::chrono::high_resolution_clock::now();

    JotLLVMBackend llvm_backend;
    auto llvm_ir_module = llvm_backend.compile(source, compilation_unit);

    auto compiling_end_time = std::chrono::high_resolution_clock::now();
    auto compiling_duration = duration_cast<std::chrono::milliseconds>(compiling_end_time - compiling_start_time);

	llvm_ir_module->print(llvm::errs(), nullptr);

    // Analysis information, later can managed with flag passed in args
    std::cout << '\n';
    jot::logd << "Analysis information from the Jot Compiler\n";
    jot::logd << "Number of nodes         : " << compilation_unit->get_tree_nodes().size() << "\n";
    jot::logd << "Parsing                 : " << parsing_duration.count() << " ms\n";
    jot::logd << "Type Checker            : " << checker_duration.count() << " ms\n";
    jot::logd << "LLVM Code Generation    : " << compiling_duration.count() << " ms\n";

    return 0;
}
