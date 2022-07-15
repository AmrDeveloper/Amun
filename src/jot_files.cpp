#include "../include/jot_files.hpp"

#include <iostream>
#include <fstream>

std::string read_file_content(const char *file_path) {
    std::ifstream ifStream(file_path);
    std::string content((std::istreambuf_iterator<char>(ifStream)), (std::istreambuf_iterator<char>()));
    return content;
}

std::string read_file_line(const char *file_path, int line_number) {
    std::ifstream file(file_path);
    int current_line_number = 1;
    for (std::string line; std::getline(file, line); current_line_number++) {
        if (current_line_number == line_number) {
            return line;
        }
    }
    return "";
}