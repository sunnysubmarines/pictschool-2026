#include "movement_layer.h"

static int write_commands(int *out, int max_commands, const int *commands, int count) {
    if (count > max_commands) {
        return -1;
    }
    for (int i = 0; i < count; ++i) {
        out[i] = commands[i];
    }
    return count;
}

int movement_is_supported_command(int command) {
    return command == SCHOOL_CMD_FORWARD ||
           command == SCHOOL_CMD_BACKWARD ||
           command == SCHOOL_CMD_TURN_LEFT ||
           command == SCHOOL_CMD_TURN_RIGHT ||
           command == SCHOOL_CMD_FORWARD_TWICE ||
           command == SCHOOL_CMD_U_TURN ||
           command == SCHOOL_CMD_STEP_RIGHT ||
           command == SCHOOL_CMD_STEP_LEFT ||
           command == SCHOOL_CMD_SQUARE;
}

int movement_expand_command(int command, int *platform_commands, int max_commands) {
    switch (command) {
        case SCHOOL_CMD_FORWARD: {
            const int commands[] = {PLATFORM_CMD_FORWARD};
            return write_commands(platform_commands, max_commands, commands, 1);
        }
        case SCHOOL_CMD_BACKWARD: {
            const int commands[] = {PLATFORM_CMD_BACKWARD};
            return write_commands(platform_commands, max_commands, commands, 1);
        }
        case SCHOOL_CMD_TURN_LEFT: {
            const int commands[] = {PLATFORM_CMD_TURN_LEFT};
            return write_commands(platform_commands, max_commands, commands, 1);
        }
        case SCHOOL_CMD_TURN_RIGHT: {
            const int commands[] = {PLATFORM_CMD_TURN_RIGHT};
            return write_commands(platform_commands, max_commands, commands, 1);
        }
        case SCHOOL_CMD_FORWARD_TWICE: {
            const int commands[] = {PLATFORM_CMD_FORWARD, PLATFORM_CMD_FORWARD};
            return write_commands(platform_commands, max_commands, commands, 2);
        }
        case SCHOOL_CMD_U_TURN: {
            const int commands[] = {PLATFORM_CMD_TURN_LEFT, PLATFORM_CMD_TURN_LEFT};
            return write_commands(platform_commands, max_commands, commands, 2);
        }
        case SCHOOL_CMD_STEP_RIGHT: {
            const int commands[] = {PLATFORM_CMD_TURN_RIGHT, PLATFORM_CMD_FORWARD, PLATFORM_CMD_TURN_LEFT};
            return write_commands(platform_commands, max_commands, commands, 3);
        }
        case SCHOOL_CMD_STEP_LEFT: {
            const int commands[] = {PLATFORM_CMD_TURN_LEFT, PLATFORM_CMD_FORWARD, PLATFORM_CMD_TURN_RIGHT};
            return write_commands(platform_commands, max_commands, commands, 3);
        }
        case SCHOOL_CMD_SQUARE: {
            const int commands[] = {
                PLATFORM_CMD_FORWARD,
                PLATFORM_CMD_TURN_RIGHT,
                PLATFORM_CMD_FORWARD,
                PLATFORM_CMD_TURN_RIGHT,
                PLATFORM_CMD_FORWARD,
                PLATFORM_CMD_TURN_RIGHT,
                PLATFORM_CMD_FORWARD,
                PLATFORM_CMD_TURN_RIGHT
            };
            return write_commands(platform_commands, max_commands, commands, 8);
        }

        default:
            return -1;
    }
}
