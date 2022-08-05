#pragma once

#include <string>

std::string read_file_content(const char *file_path);

std::string read_file_line(const char *file_path, int line_number);

void create_file_with_content(const std::string &path, const std::string &content);

void create_new_directory(const std::string &path);

bool is_file_exists(const std::string &path);