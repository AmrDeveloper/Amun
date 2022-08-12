#pragma once

#include "jot_token.hpp"

#include <vector>

class JotDiagnostic {
  public:
    JotDiagnostic(TokenSpan location, std::string message)
        : location(std::move(location)), message(std::move(message)) {}

    TokenSpan get_location() { return location; }

    std::string get_message() { return message; }

  private:
    TokenSpan location;
    std::string message;
};

class JotDiagnosticEngine {
  public:
    void report_diagnostics();

    void add_diagnostic(TokenSpan location, std::string message);

    int diagnostics_size();

  private:
    void report_diagnostic(JotDiagnostic &diagnostic);

    std::vector<JotDiagnostic> diagnostics;
};