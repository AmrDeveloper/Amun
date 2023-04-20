#pragma once

#include <functional>
#include <iostream>
#include <string_view>
#include <unordered_map>

namespace amun {

class CommandMap {
  public:
    auto registerCommand(const char* name, const std::function<int(int, char**)>& command) -> void;

    auto executeCommand(int argc, char** argv) -> int;

  private:
    std::unordered_map<std::string_view, std::function<int(int, char**)>> commands_map;
};

} // namespace amun