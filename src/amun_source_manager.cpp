#include "../include/amun_source_manager.hpp"

auto amun::SourceManager::register_source_path(std::string path) -> int
{
    last_source_file_id++;
    files_map[last_source_file_id] = path;
    files_set.insert(path);
    return last_source_file_id;
}

auto amun::SourceManager::resolve_source_path(int source_id) -> std::string
{
    return files_map[source_id];
}

auto amun::SourceManager::is_path_registered(std::string path) -> bool
{
    return files_set.contains(path);
}
