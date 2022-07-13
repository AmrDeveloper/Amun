#include <iostream>

#include "../include/jot_tokenizer.hpp"
#include "../include/jot_logger.hpp"

int main(int argc, char** argv) {
    if (argc < 2) {
        printf("Usage: %s <source_code>", argv[0]);
        return EXIT_FAILURE;
    }
    std::string source = std::string(argv[1]);
    std::string file_name = "main.jot";
    JotTokenizer tokenizer(file_name, source);

    jot::logger << "Start tokenizer\n";
    auto token = tokenizer.scan_next_token(); 
    while (!token.is_end_of_file()) {
        std::cout << "Token -> " << token.get_kind_literal() << std::endl;
        token = tokenizer.scan_next_token();
    } 
    
    jot::logger << "Finish tokenizer\n";
    return 0;
}
