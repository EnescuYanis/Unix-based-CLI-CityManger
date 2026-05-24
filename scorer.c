#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include "commands.h"

#define MAX_INSPECTORS 128

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: scorer <district_id>\n");
        return 1;
    }

    const char *district_id = argv[1];
    char file_path[2048];
    snprintf(file_path, sizeof(file_path), "%s/reports.dat", district_id);

    int fd = open(file_path, O_RDONLY);
    if (fd == -1) {
        printf("District '%s': reports.dat not found\n", district_id);
        return 0;
    }

    char names[MAX_INSPECTORS][50];
    int scores[MAX_INSPECTORS];
    int counts[MAX_INSPECTORS];
    int num = 0;

    Record r;
    while (read(fd, &r, sizeof(Record)) == (ssize_t)sizeof(Record)) {
        int found = 0;
        for (int i = 0; i < num; i++) {
            if (strcmp(names[i], r.inspector_name) == 0) {
                scores[i] += r.security_level;
                counts[i]++;
                found = 1;
                break;
            }
        }
        if (!found && num < MAX_INSPECTORS) {
            strncpy(names[num], r.inspector_name, 49);
            names[num][49] = '\0';
            scores[num] = r.security_level;
            counts[num] = 1;
            num++;
        }
    }
    close(fd);

    printf("District: %s\n", district_id);
    if (num == 0) {
        printf("  (no reports)\n");
    } else {
        for (int i = 0; i < num; i++) {
            printf("  %-20s  score=%-4d  reports=%d\n",
                   names[i], scores[i], counts[i]);
        }
    }

    return 0;
}
