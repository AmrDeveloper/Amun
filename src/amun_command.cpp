#include "../include/amun_command.hpp"

#include <cstdio>

auto amun::CommandMap::registerCommand(const char* name,
                                       const std::function<int(int, char**)>& command) -> void
{
    commands_map[name] = command;
}

auto amun::CommandMap::executeCommand(int argc, char** argv) -> int
{
    if (argc < 2) {
        printf("Usage: %s <command> <options>\n", argv[0]);
        return EXIT_FAILURE;
    }

    const char* command = argv[1];
    if (!commands_map.contains(command)) {
        printf("Can't find command with name %s\n", command);
        printf("Please run amun help to get the list of available commands\n");
        return EXIT_FAILURE;
    }

    return commands_map[command](argc, argv);
}
