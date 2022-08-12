#include "../include/jot_diagnostics.hpp"
#include "../include/jot_files.hpp"

#include <iostream>

void JotDiagnosticEngine::report_diagnostics() {
    for (auto &diagnostic : diagnostics) {
        report_diagnostic(diagnostic);
    }
}

void JotDiagnosticEngine::report_diagnostic(JotDiagnostic &diagnostic) {
    auto location = diagnostic.get_location();
    auto message = diagnostic.get_message();
    auto file_name = location.get_file_name();
    auto source_line = read_file_line(file_name.data(), location.get_line_number());

    std::cout << "Error in " << file_name << std::endl;
    std::cout << "At line " << location.get_line_number() << " and column "
              << location.get_column_start() << std::endl;
    ;

    std::cout << source_line << std::endl;

    for (size_t i = 0; i < location.get_column_start(); i++) {
        std::cout << "~";
    }

    std::cout << "^ " << message << std::endl;
    std::cout << std::endl;
}

void JotDiagnosticEngine::add_diagnostic(TokenSpan location, std::string message) {
    diagnostics.push_back({location, message});
}

int JotDiagnosticEngine::diagnostics_size() { return diagnostics.size(); }