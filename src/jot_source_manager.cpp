#include "../include/jot_source_manager.hpp"
#include <cstring>

int JotSourceManager::register_source_path(const char* path)
{
    last_source_file_id++;
    files_map[last_source_file_id] = path;
    return last_source_file_id;
}

const char* JotSourceManager::resolve_source_path(int source_id) { return files_map[source_id]; }

bool JotSourceManager::is_path_registered(const char* path)
{
    for (int i = 0; i <= last_source_file_id; i++) {
        if (std::strcmp(files_map[last_source_file_id], path) == 0)
            return true;
    }
    return false;
}
