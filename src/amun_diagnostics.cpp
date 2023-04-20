#include "../include/amun_diagnostics.hpp"
#include "../include/amun_files.hpp"

#include <iostream>
#include <string>

amun::DiagnosticEngine::DiagnosticEngine(amun::SourceManager& source_manager)
    : source_manager(source_manager)
{
    diagnostics.reserve(DIAGNOSTIC_LEVEL_COUNT);
}

auto amun::DiagnosticEngine::report_diagnostics(DiagnosticLevel level) -> void
{
    auto kind_diagnostics = diagnostics[level];
    for (auto& diagnostic : kind_diagnostics) {
        report_diagnostic(diagnostic);
    }
}

auto amun::DiagnosticEngine::report_diagnostic(amun::Diagnostic& diagnostic) -> void
{
    auto location = diagnostic.location;
    auto message = diagnostic.message;
    auto file_name = source_manager.resolve_source_path(location.file_id);
    auto line_number = location.line_number;
    auto source_line = amun::read_file_line(file_name.c_str(), line_number);

    const auto* kind_literal = diagnostic_level_literal[diagnostic.level];

    std::cout << kind_literal << " in " << file_name << ':' << std::to_string(line_number) << ':'
              << std::to_string(location.column_start) << std::endl;

    auto line_number_header = std::to_string(line_number) + " | ";
    std::cout << line_number_header << source_line << std::endl;

    auto header_size = line_number_header.size();
    for (size_t i = 0; i < location.column_start + header_size; i++) {
        std::cout << "~";
    }

    std::cout << "^ " << message << std::endl;
    std::cout << std::endl;
}

auto amun::DiagnosticEngine::report_error(TokenSpan location, std::string message) -> void
{
    diagnostics[DiagnosticLevel::ERROR].push_back({location, message, DiagnosticLevel::ERROR});
}

auto amun::DiagnosticEngine::report_warning(TokenSpan location, std::string message) -> void
{
    diagnostics[DiagnosticLevel::WARNING].push_back({location, message, DiagnosticLevel::WARNING});
}

auto amun::DiagnosticEngine::level_count(DiagnosticLevel level) -> int64
{
    if (diagnostics.find(level) == diagnostics.end()) {
        return 0;
    }
    return diagnostics[level].size();
}