#include "../include/jot_files.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>

std::string read_file_content(const char* file_path)
{
    std::ifstream ifStream(file_path);
    std::string content((std::istreambuf_iterator<char>(ifStream)),
                        (std::istreambuf_iterator<char>()));
    return content;
}

std::string read_file_line(const char* file_path, int line_number)
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

void create_file_with_content(const std::string& path, const std::string& content)
{
    std::ofstream of_stream(path);
    of_stream << content;
    of_stream.close();
}

void create_new_directory(const std::string& path) { std::filesystem::create_directory(path); }

std::string find_parent_path(const std::string& path)
{
    std::filesystem::path file_system_path(path);
    return file_system_path.parent_path().string();
}

bool is_file_exists(const std::string& path) { return std::filesystem::exists(path); }