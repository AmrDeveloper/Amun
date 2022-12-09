#include "../include/jot_diagnostics.hpp"
#include "../include/jot_files.hpp"

#include <iostream>
#include <string>

JotDiagnosticEngine::JotDiagnosticEngine(JotSourceManager& source_manager)
    : source_manager(source_manager)
{
}

void JotDiagnosticEngine::report_diagnostics(DiagnosticLevel level)
{
    for (auto& diagnostic : diagnostics) {
        if (diagnostic.get_level() == level) {
            report_diagnostic(diagnostic);
        }
    }
}

void JotDiagnosticEngine::report_diagnostic(JotDiagnostic& diagnostic)
{
    auto location = diagnostic.get_location();
    auto message = diagnostic.get_message();
    auto file_name = source_manager.resolve_source_path(location.file_id);
    auto line_number = location.line_number;
    auto source_line = read_file_line(file_name, line_number);

    std::cout << diagnostic.get_level_literal() << " in " << file_name << ':'
              << std::to_string(line_number) << ':' << std::to_string(location.column_start)
              << std::endl;

    auto line_number_header = std::to_string(line_number) + " | ";
    std::cout << line_number_header << source_line << std::endl;

    auto header_size = line_number_header.size();
    for (size_t i = 0; i < location.column_start + header_size; i++) {
        std::cout << "~";
    }

    std::cout << "^ " << message << std::endl;
    std::cout << std::endl;
}

void JotDiagnosticEngine::add_diagnostic_error(TokenSpan location, std::string message)
{
    errors_number++;
    diagnostics.push_back({location, message, DiagnosticLevel::Error});
}

void JotDiagnosticEngine::add_diagnostic_warn(TokenSpan location, std::string message)
{
    warns_number++;
    diagnostics.push_back({location, message, DiagnosticLevel::Warning});
}

int JotDiagnosticEngine::get_warns_number() { return warns_number; }

int JotDiagnosticEngine::get_errors_number() { return errors_number; }