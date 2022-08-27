#include "../include/jot_diagnostics.hpp"
#include "../include/jot_files.hpp"

#include <iostream>
#include <string>

void JotDiagnosticEngine::report_diagnostics() {
    for (auto &diagnostic : diagnostics) {
        report_diagnostic(diagnostic);
    }
}

void JotDiagnosticEngine::report_diagnostic(JotDiagnostic &diagnostic) {
    auto location = diagnostic.get_location();
    auto message = diagnostic.get_message();
    auto file_name = location.get_file_name();
    auto line_number = location.get_line_number();
    auto source_line = read_file_line(file_name.data(), line_number);

    std::cout << "Error in " << file_name << ':' << std::to_string(line_number) << ':'
              << std::to_string(location.get_column_start()) << std::endl;
    auto line_number_header = std::to_string(line_number) + " | ";
    std::cout << line_number_header << source_line << std::endl;

    auto header_size = line_number_header.size();
    for (size_t i = 0; i < location.get_column_start() + header_size; i++) {
        std::cout << "~";
    }

    std::cout << "^ " << message << std::endl;
    std::cout << std::endl;
}

void JotDiagnosticEngine::add_diagnostic(TokenSpan location, std::string message) {
    diagnostics.push_back({location, message});
}

int JotDiagnosticEngine::diagnostics_size() { return diagnostics.size(); }