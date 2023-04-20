#pragma once

#include "amun_source_manager.hpp"
#include "amun_token.hpp"

#include <unordered_map>
#include <vector>

namespace amun {

enum class DiagnosticLevel { WARNING, ERROR };

static std::unordered_map<DiagnosticLevel, const char*> diagnostic_level_literal = {
    {DiagnosticLevel::WARNING, "WARNING"},
    {DiagnosticLevel::ERROR, "ERROR"},
};

constexpr const auto DIAGNOSTIC_LEVEL_COUNT = 2;

struct Diagnostic {
    Diagnostic(TokenSpan location, std::string message, DiagnosticLevel level)
        : location(location), message(std::move(message)), level(level)
    {
    }

    TokenSpan location;
    std::string message;
    DiagnosticLevel level;
};

class DiagnosticEngine {
  public:
    explicit DiagnosticEngine(amun::SourceManager& source_manager);

    auto report_diagnostics(DiagnosticLevel level) -> void;

    auto report_error(TokenSpan location, std::string message) -> void;

    auto report_warning(TokenSpan location, std::string message) -> void;

    auto level_count(DiagnosticLevel level) -> int64;

  private:
    auto report_diagnostic(Diagnostic& diagnostic) -> void;

    amun::SourceManager& source_manager;

    std::unordered_map<DiagnosticLevel, std::vector<Diagnostic>> diagnostics;
};

} // namespace amun