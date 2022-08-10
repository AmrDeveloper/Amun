#pragma once

#include <unordered_set>

class JotContext {
  public:
    bool is_path_visited(std::string &path);

    void set_path_visited(std::string &path);

    bool is_prefix_function(std::string &name);

    void set_prefix_function(std::string &name);

    bool is_infix_function(std::string &name);

    void set_infix_function(std::string &name);

  private:
    std::unordered_set<std::string> visited_files;
    std::unordered_set<std::string> prefix_functions;
    std::unordered_set<std::string> infix_functions;
};