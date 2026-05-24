#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>

#define MONITOR_BIN "./monitor_reports"
#define SCORER_BIN  "./scorer"
#define MAX_DISTRICTS 64

static pid_t hub_mon_pid = -1;

/* Read one '\n'-terminated line from fd into buf (returns chars read, -1 on EOF/error) */
static int read_line(int fd, char *buf, int maxlen) {
    int i = 0;
    char c;
    while (i < maxlen - 1) {
        ssize_t n = read(fd, &c, 1);
        if (n <= 0) return i > 0 ? i : -1;
        buf[i++] = c;
        if (c == '\n') break;
    }
    buf[i] = '\0';
    return i;
}

/*
 * hub_mon: runs as a background child of city_hub.
 * It creates a pipe, forks monitor_reports with its stdout redirected
 * to the pipe's write end, then reads and displays messages.
 */
static void run_hub_mon(void) {
    int pipefd[2];
    if (pipe(pipefd) == -1) { perror("pipe"); exit(1); }

    pid_t mon_pid = fork();
    if (mon_pid == -1) { perror("fork"); exit(1); }

    if (mon_pid == 0) {
        /* child → monitor_reports: redirect stdout to pipe write end */
        close(pipefd[0]);
        if (dup2(pipefd[1], STDOUT_FILENO) == -1) { perror("dup2"); exit(1); }
        close(pipefd[1]);
        execl(MONITOR_BIN, MONITOR_BIN, (char *)NULL);
        perror("execl monitor_reports");
        exit(1);
    }

    /* hub_mon: close write end, read messages from read end */
    close(pipefd[1]);

    char line[512];
    while (read_line(pipefd[0], line, sizeof(line)) > 0) {
        /* Strip trailing newline for display */
        line[strcspn(line, "\n")] = '\0';

        if (strncmp(line, "START:", 6) == 0) {
            printf("\n[monitor] %s\n", line + 6);
        } else if (strncmp(line, "REPORT:", 7) == 0) {
            printf("\n[monitor] *** %s ***\n", line + 7);
        } else if (strncmp(line, "END:", 4) == 0) {
            printf("\n[monitor] %s\n", line + 4);
            printf("[hub_mon] Monitor has ended — hub_mon exiting.\n");
            fflush(stdout);
            break;
        } else if (strncmp(line, "ERROR:", 6) == 0) {
            printf("\n[monitor] ERROR: %s\n", line + 6);
            printf("[hub_mon] Monitor could not start — hub_mon exiting.\n");
            fflush(stdout);
            break;
        } else {
            /* Unrecognised line — pass through */
            printf("\n[monitor] %s\n", line);
        }
        fflush(stdout);
    }

    close(pipefd[0]);
    waitpid(mon_pid, NULL, 0);
    exit(0);
}

static void cmd_start_monitor(void) {
    /* Check if an existing hub_mon is still alive */
    if (hub_mon_pid != -1 && kill(hub_mon_pid, 0) == 0) {
        printf("Monitor is already running (hub_mon PID=%d).\n", (int)hub_mon_pid);
        return;
    }

    hub_mon_pid = fork();
    if (hub_mon_pid == -1) { perror("fork"); return; }

    if (hub_mon_pid == 0) {
        run_hub_mon(); /* never returns */
        exit(0);
    }

    printf("Monitor started (hub_mon PID=%d).\n", (int)hub_mon_pid);
}

static void cmd_calculate_scores(char *args) {
    char *districts[MAX_DISTRICTS];
    int n = 0;

    char *tok = strtok(args, " \t");
    while (tok && n < MAX_DISTRICTS) {
        districts[n++] = tok;
        tok = strtok(NULL, " \t");
    }

    if (n == 0) {
        printf("Usage: calculate_scores <district1> [district2 ...]\n");
        return;
    }

    int pipefd[MAX_DISTRICTS][2];
    pid_t pids[MAX_DISTRICTS];

    /* Spawn one scorer per district */
    for (int i = 0; i < n; i++) {
        if (pipe(pipefd[i]) == -1) { perror("pipe"); return; }

        pids[i] = fork();
        if (pids[i] == -1) { perror("fork"); return; }

        if (pids[i] == 0) {
            /* child: redirect stdout → pipe write end, then exec scorer */
            close(pipefd[i][0]);
            if (dup2(pipefd[i][1], STDOUT_FILENO) == -1) { perror("dup2"); exit(1); }
            close(pipefd[i][1]);
            execl(SCORER_BIN, SCORER_BIN, districts[i], (char *)NULL);
            perror("execl scorer");
            exit(1);
        }

        close(pipefd[i][1]); /* parent closes write end */
    }

    /* Collect and print all scorer output */
    printf("\n=== Workload Report ===\n");
    char buf[4096];
    for (int i = 0; i < n; i++) {
        ssize_t bytes;
        while ((bytes = read(pipefd[i][0], buf, sizeof(buf) - 1)) > 0) {
            buf[bytes] = '\0';
            printf("%s", buf);
        }
        close(pipefd[i][0]);
        waitpid(pids[i], NULL, 0);
    }
    printf("=======================\n\n");
}

int main(void) {
    printf("CityHub — interactive console\n");
    printf("Type 'help' for available commands.\n\n");

    char line[1024];
    while (1) {
        printf("cityhub> ");
        fflush(stdout);

        if (!fgets(line, sizeof(line), stdin)) break;
        line[strcspn(line, "\n")] = '\0';
        if (strlen(line) == 0) continue;

        if (strcmp(line, "exit") == 0 || strcmp(line, "quit") == 0) {
            printf("Goodbye.\n");
            break;
        } else if (strcmp(line, "start_monitor") == 0) {
            cmd_start_monitor();
        } else if (strncmp(line, "calculate_scores", 16) == 0) {
            char *args = line + 16;
            while (*args == ' ' || *args == '\t') args++;
            cmd_calculate_scores(args);
        } else if (strcmp(line, "help") == 0) {
            printf("Commands:\n");
            printf("  start_monitor                  Start the monitor background process\n");
            printf("  calculate_scores <d1> [d2...]  Per-inspector workload scores\n");
            printf("  exit / quit                    Exit cityhub\n\n");
        } else {
            printf("Unknown command '%s'. Type 'help'.\n", line);
        }
    }

    return 0;
}
