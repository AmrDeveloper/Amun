#pragma once

#include <functional>
#include <iostream>
#include <unordered_map>

class JotCommands {
  public:
    void registerCommand(const char* name, const std::function<int(int, char**)>& command);

    int executeCommand(int argc, char** argv);

  private:
    std::unordered_map<std::string, std::function<int(int, char**)>> commands_map;
};
