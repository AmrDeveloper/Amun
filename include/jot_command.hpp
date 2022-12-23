#pragma once

#include <functional>
#include <iostream>
#include <unordered_map>

using CommandFunction = std::function<int(int, char**)>;

class JotCommands {
  public:
    void registerCommand(const char* name, const CommandFunction& command);

    int executeCommand(int argc, char** argv);

  private:
    std::unordered_map<std::string_view, CommandFunction> commands_map;
};
