#pragma once

#include <string_view>
#include <unordered_map>

class JotSourceManager {
  public:
    int register_source_path(const char* path);

    const char* resolve_source_path(int source_id);

    bool is_path_registered(const char* path);

  private:
    std::unordered_map<int, const char*> files_map;
    int                                  last_source_file_id = -1;
};