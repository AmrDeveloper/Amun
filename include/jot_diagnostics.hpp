#pragma once

#include "jot_source_manager.hpp"
#include "jot_token.hpp"

#include <unordered_map>
#include <vector>

enum class DiagnosticLevel { WARNING, ERROR };

static std::unordered_map<DiagnosticLevel, const char*> diagnostic_level_literal = {
    {DiagnosticLevel::WARNING, "WARNING"},
    {DiagnosticLevel::ERROR, "ERROR"},
};

constexpr const auto DIAGNOSTIC_LEVEL_COUNT = 2;

struct JotDiagnostic {
    JotDiagnostic(TokenSpan location, std::string message, DiagnosticLevel level)
        : location(location), message(std::move(message)), level(level)
    {
    }

    TokenSpan       location;
    std::string     message;
    DiagnosticLevel level;
};

class JotDiagnosticEngine {
  public:
    explicit JotDiagnosticEngine(JotSourceManager& source_manager);

    auto report_diagnostics(DiagnosticLevel level) -> void;

    auto report_error(TokenSpan location, std::string message) -> void;

    auto report_warning(TokenSpan location, std::string message) -> void;

    auto level_count(DiagnosticLevel level) -> int64;

  private:
    auto report_diagnostic(JotDiagnostic& diagnostic) -> void;

    JotSourceManager& source_manager;

    std::unordered_map<DiagnosticLevel, std::vector<JotDiagnostic>> diagnostics;
};