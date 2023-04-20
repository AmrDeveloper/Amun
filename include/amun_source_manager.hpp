#pragma once

#include <string>
#include <unordered_map>
#include <unordered_set>

namespace amun {

class SourceManager {
  public:
    auto register_source_path(std::string path) -> int;

    auto resolve_source_path(int source_id) -> std::string;

    auto is_path_registered(std::string path) -> bool;

  private:
    std::unordered_map<int, std::string> files_map;
    std::unordered_set<std::string> files_set;
    int last_source_file_id = -1;
};

} // namespace amun