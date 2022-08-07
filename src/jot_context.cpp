#include "../include/jot_context.hpp"

bool JotContext::is_path_visited(std::string &path) { return visited_files.count(path); }

void JotContext::set_path_visited(std::string &path) { visited_files.insert(path); }