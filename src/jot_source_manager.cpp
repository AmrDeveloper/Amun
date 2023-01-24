#include "../include/jot_source_manager.hpp"

int JotSourceManager::register_source_path(std::string path)
{
    last_source_file_id++;
    files_map[last_source_file_id] = path;
    files_set.insert(path);
    return last_source_file_id;
}

std::string JotSourceManager::resolve_source_path(int source_id) { return files_map[source_id]; }

bool JotSourceManager::is_path_registered(std::string path) { return files_set.contains(path); }
