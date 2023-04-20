#include "../include/amun_files.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>

auto amun::read_file_content(const char* file_path) -> std::string
{
    std::ifstream if_Stream(file_path);
    std::string content((std::istreambuf_iterator<char>(if_Stream)),
                        (std::istreambuf_iterator<char>()));
    return content;
}

auto amun::read_file_line(const char* file_path, int line_number) -> std::string
{
    std::ifstream file(file_path);
    int current_line_number = 1;
    for (std::string line; std::getline(file, line); current_line_number++) {
        if (current_line_number == line_number) {
            return line;
        }
    }
    return "";
}

auto amun::create_file_with_content(const std::string& path, const std::string& content) -> void
{
    std::ofstream of_stream(path);
    of_stream << content;
    of_stream.close();
}

auto amun::create_new_directory(const std::string& path) -> void
{
    std::filesystem::create_directory(path);
}

auto amun::find_parent_path(const std::string& path) -> std::string
{
    std::filesystem::path file_system_path(path);
    return file_system_path.parent_path().string();
}

auto amun::is_file_exists(const std::string& path) -> bool { return std::filesystem::exists(path); }