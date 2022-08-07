#pragma once

#include <unordered_set>

class JotContext {
  public:
    bool is_path_visited(std::string &path);

    void set_path_visited(std::string &path);

  private:
    std::unordered_set<std::string> visited_files;
};