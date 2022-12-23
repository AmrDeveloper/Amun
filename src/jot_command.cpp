#include "../include/jot_command.hpp"
#include <cstdio>

void JotCommands::registerCommand(const char* literal, const CommandFunction& command)
{
    commands_map[literal] = command;
}

int JotCommands::executeCommand(int argc, char** argv)
{
    if (argc < 2) {
        printf("Usage: %s <command> <options>\n", argv[0]);
        return EXIT_FAILURE;
    }

    const char* command = argv[1];
    if (not commands_map.count(command)) {
        printf("Can't find command with name %s\n", command);
        printf("Please run jot help to get the list of available commands\n");
        return EXIT_FAILURE;
    }

    return commands_map[command](argc, argv);
}
