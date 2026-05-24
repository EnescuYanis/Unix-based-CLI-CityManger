#include "commands.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

int main(int argc, char *argv[]) {
    srand(time(NULL));

    char *role    = NULL;
    char *user    = NULL;
    char *command = NULL;
    char *district = NULL;
    char *extra_arg = NULL;

    // Condition arguments for --filter (everything after the district)
    char **conditions = NULL;
    int    num_conditions = 0;

    if (argc < 2) {
        printf("Error: No arguments provided.\n");
        handle_help();
        return 1;
    }

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--role") == 0 && i + 1 < argc) {
            role = argv[++i];
        } else if (strcmp(argv[i], "--user") == 0 && i + 1 < argc) {
            user = argv[++i];
        } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "help") == 0) {
            handle_help();
            return 0;
        } else if (strncmp(argv[i], "--", 2) == 0) {
            command = argv[i] + 2; // strip "--"

            // First positional arg after the command = district
            if (i + 1 < argc && argv[i+1][0] != '-') district = argv[++i];

            if (strcmp(command, "filter") == 0) {
                // All remaining non-flag args are conditions
                conditions = &argv[i + 1];
                num_conditions = 0;
                while (i + 1 < argc && argv[i+1][0] != '-') {
                    num_conditions++;
                    i++;
                }
            } else {
                // At most one extra arg (e.g., report_id or threshold value)
                if (i + 1 < argc && argv[i+1][0] != '-') extra_arg = argv[++i];
            }
        }
    }

    // Mandatory argument check
    if (!role || !user || !command) {
        printf(COLOR_RED "[ERROR]" COLOR_RESET " Missing mandatory arguments.\n");
        printf("Usage: ./city_manager --role <manager|inspector> --user <name> --<command> [args...]\n");
        return 1;
    }

    // Validate role
    if (strcmp(role, "manager") != 0 && strcmp(role, "inspector") != 0) {
        printf(COLOR_RED "[ERROR]" COLOR_RESET " Invalid role '%s'. Must be 'manager' or 'inspector'.\n", role);
        return 1;
    }

    // Command dispatcher
    if (strcmp(command, "add") == 0) {
        if (!district) { printf(COLOR_RED "[ERROR]" COLOR_RESET " --add requires <district_id>.\n"); return 1; }
        handle_add(district, role, user);

    }
    else if (strcmp(command, "remove_district") == 0) {
        if (!district) { printf(COLOR_RED "[ERROR]" COLOR_RESET " --remove_district requires <district_id>.\n"); return 1; }
        handle_remove_district(district, role, user);
    }
    else if (strcmp(command, "remove_report") == 0) {
        if (!district || !extra_arg) {
            printf(COLOR_RED "[ERROR]" COLOR_RESET " --remove_report requires <district_id> <report_id>.\n"); return 1;
        }
        handle_remove_report(district, extra_arg, role, user);
    }
    else if (strcmp(command, "list") == 0) {
        if (!district) { printf(COLOR_RED "[ERROR]" COLOR_RESET " --list requires <district_id>.\n"); return 1; }
        handle_list(district, role, user);

    } else if (strcmp(command, "view") == 0) {
        if (!district || !extra_arg) {
            printf(COLOR_RED "[ERROR]" COLOR_RESET " --view requires <district_id> <report_id>.\n"); return 1;
        }
        handle_view(district, extra_arg, role, user);

    } else if (strcmp(command, "update_threshold") == 0) {
        if (!district || !extra_arg) {
            printf(COLOR_RED "[ERROR]" COLOR_RESET " --update_threshold requires <district_id> <value>.\n"); return 1;
        }
        handle_update_threshold(district, extra_arg, role, user);

    } else if (strcmp(command, "filter") == 0) {
        if (!district || num_conditions == 0) {
            printf(COLOR_RED "[ERROR]" COLOR_RESET " --filter requires <district_id> and at least one condition.\n"); return 1;
        }
        handle_filter(district, num_conditions, conditions, role, user);

    } else {
        printf(COLOR_RED "[ERROR]" COLOR_RESET " Unknown command '--%s'.\n", command);
        handle_help();
        return 1;
    }

    return 0;
}