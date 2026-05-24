#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>

#define PID_FILE ".monitor_pid"

static volatile sig_atomic_t running = 1;

/* Write a typed message: "TYPE:text\n" — works on both terminal and pipe */
static void emit(const char *type, const char *text) {
    printf("%s:%s\n", type, text);
    fflush(stdout);
}

static void handle_sigusr1(int sig) {
    (void)sig;
    emit("REPORT", "New report has been added to the system");
}

static void handle_sigint(int sig) {
    (void)sig;
    running = 0;
}

int main(void) {
    /* Disable buffering so pipe readers get messages immediately */
    setvbuf(stdout, NULL, _IONBF, 0);

    /* Check if another monitor is already running */
    int check_fd = open(PID_FILE, O_RDONLY);
    if (check_fd != -1) {
        char buf[32];
        ssize_t n = read(check_fd, buf, sizeof(buf) - 1);
        close(check_fd);
        if (n > 0) {
            buf[n] = '\0';
            pid_t existing = (pid_t)atoi(buf);
            if (kill(existing, 0) == 0) {
                char msg[64];
                snprintf(msg, sizeof(msg), "Monitor already running with PID %d", (int)existing);
                emit("ERROR", msg);
                return 1;
            }
        }
    }

    /* Write our own PID */
    int pid_fd = open(PID_FILE, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (pid_fd == -1) {
        perror("open .monitor_pid");
        return 1;
    }
    char pid_str[32];
    snprintf(pid_str, sizeof(pid_str), "%d\n", (int)getpid());
    write(pid_fd, pid_str, strlen(pid_str));
    close(pid_fd);

    char start_msg[64];
    snprintf(start_msg, sizeof(start_msg), "Monitor started with PID %d", (int)getpid());
    emit("START", start_msg);

    /* Signal handlers via sigaction (no signal() per spec) */
    struct sigaction sa_usr1, sa_int;

    memset(&sa_usr1, 0, sizeof(sa_usr1));
    sa_usr1.sa_handler = handle_sigusr1;
    sigemptyset(&sa_usr1.sa_mask);
    sa_usr1.sa_flags = SA_RESTART;
    sigaction(SIGUSR1, &sa_usr1, NULL);

    memset(&sa_int, 0, sizeof(sa_int));
    sa_int.sa_handler = handle_sigint;
    sigemptyset(&sa_int.sa_mask);
    sigaction(SIGINT, &sa_int, NULL);

    while (running)
        pause();

    emit("END", "Monitor received SIGINT — shutting down");
    unlink(PID_FILE);
    return 0;
}
