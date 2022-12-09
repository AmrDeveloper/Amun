#pragma once

#include "jot_diagnostics.hpp"
#include "jot_options.hpp"
#include "jot_source_manager.hpp"
#include "jot_type.hpp"

#include <memory>
#include <unordered_set>

class JotContext {
  public:
    JotContext();

    bool is_prefix_function(std::string& name);

    void set_prefix_function(std::string& name);

    bool is_infix_function(std::string& name);

    void set_infix_function(std::string& name);

    bool is_postfix_function(std::string& name);

    void set_postfix_function(std::string& name);

    JotOptions                                                      options;
    JotDiagnosticEngine                                             diagnostics;
    JotSourceManager                                                source_manager;
    std::unordered_map<std::string, std::shared_ptr<JotStructType>> structures;
    std::unordered_map<std::string, std::shared_ptr<JotEnumType>>   enumerations;

  private:
    std::unordered_set<std::string> prefix_functions;
    std::unordered_set<std::string> infix_functions;
    std::unordered_set<std::string> postfix_functions;
};