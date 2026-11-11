#include "../headers/header.h"

int main(int argc, char **argv)
{
    char command[100];
    Node node = {0};
    while (1)
    {
        if (fgets(command, sizeof(command), stdin) == NULL)
            break ;
        process_command(&node, command);
    }
    return (0);
}