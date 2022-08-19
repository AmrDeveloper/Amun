#pragma once

#include "jot_diagnostics.hpp"
#include "jot_type.hpp"

#include <memory>
#include <unordered_set>


class JotContext {
  public:
    bool is_path_visited(std::string &path);

    void set_path_visited(std::string &path);

    bool is_prefix_function(std::string &name);

    void set_prefix_function(std::string &name);

    bool is_infix_function(std::string &name);

    void set_infix_function(std::string &name);

    bool is_postfix_function(std::string &name);

    void set_postfix_function(std::string &name);

    JotDiagnosticEngine diagnostics;
    std::unordered_map<std::string, std::shared_ptr<JotEnumType>> enumerations;

  private:
    std::unordered_set<std::string> visited_files;
    std::unordered_set<std::string> prefix_functions;
    std::unordered_set<std::string> infix_functions;
    std::unordered_set<std::string> postfix_functions;
};