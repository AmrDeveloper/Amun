#include "../include/jot_command.hpp"

#include <cstdio>

auto JotCommands::registerCommand(const char* name, const CommandFunction& command) -> void
{
    commands_map[name] = command;
}

auto JotCommands::executeCommand(int argc, char** argv) -> int
{
    if (argc < 2) {
        printf("Usage: %s <command> <options>\n", argv[0]);
        return EXIT_FAILURE;
    }

    const char* command = argv[1];
    if (not commands_map.contains(command)) {
        printf("Can't find command with name %s\n", command);
        printf("Please run jot help to get the list of available commands\n");
        return EXIT_FAILURE;
    }

    return commands_map[command](argc, argv);
}
