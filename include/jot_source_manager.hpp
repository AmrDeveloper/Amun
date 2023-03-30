#pragma once

#include <string>
#include <unordered_map>
#include <unordered_set>

class JotSourceManager {
  public:
    int register_source_path(std::string path);

    std::string resolve_source_path(int source_id);

    bool is_path_registered(std::string path);

  private:
    std::unordered_map<int, std::string> files_map;
    std::unordered_set<std::string> files_set;
    int last_source_file_id = -1;
};