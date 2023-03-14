#pragma once

#include <functional>
#include <iostream>
#include <string_view>
#include <unordered_map>

using CommandFunction = std::function<int(int, char**)>;

class JotCommands {
  public:
    auto registerCommand(const char* name, const CommandFunction& command) -> void;

    auto executeCommand(int argc, char** argv) -> int;

  private:
    std::unordered_map<std::string_view, CommandFunction> commands_map;
};
