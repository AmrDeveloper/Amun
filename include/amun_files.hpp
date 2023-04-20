#pragma once

#include <string>

namespace amun {

auto read_file_content(const char* file_path) -> std::string;

auto read_file_line(const char* file_path, int line_number) -> std::string;

auto create_file_with_content(const std::string& path, const std::string& content) -> void;

auto create_new_directory(const std::string& path) -> void;

auto find_parent_path(const std::string& path) -> std::string;

auto is_file_exists(const std::string& path) -> bool;

} // namespace amun