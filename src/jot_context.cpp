#include "../include/jot_context.hpp"

bool JotContext::is_path_visited(std::string& path) { return visited_files.count(path); }

void JotContext::set_path_visited(std::string& path) { visited_files.insert(path); }

bool JotContext::is_prefix_function(std::string& name) { return prefix_functions.count(name); }

void JotContext::set_prefix_function(std::string& name) { prefix_functions.insert(name); }

bool JotContext::is_infix_function(std::string& name) { return infix_functions.count(name); }

void JotContext::set_infix_function(std::string& name) { infix_functions.insert(name); }

bool JotContext::is_postfix_function(std::string& name) { return postfix_functions.count(name); }

void JotContext::set_postfix_function(std::string& name) { postfix_functions.insert(name); }