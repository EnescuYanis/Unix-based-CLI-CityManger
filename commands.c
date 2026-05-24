#include "commands.h"
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>

// ============================================================
//  VISUAL UI HELPERS
// ============================================================

void print_banner(const char *role, const char *user) {
    printf("\n");
    printf(COLOR_CYAN STYLE_BOLD);
    printf("   ██████╗ ███╗   ███╗\n");
    printf("  ██╔════╝ ████╗ ████║\n");
    printf("  ██║      ██╔████╔██║\n");
    printf("  ██║      ██║╚██╔╝██║\n");
    printf("  ╚██████╗ ██║ ╚═╝ ██║\n");
    printf("   ╚═════╝ ╚═╝     ╚═╝\n");
    printf(COLOR_RESET);
    printf("  " COLOR_WHITE "City Manager" COLOR_RESET
           STYLE_DIM "  — Urban Infrastructure Reports\n" COLOR_RESET);
    printf("\n");

    if (strcmp(role, "manager") == 0) {
        printf("  " COLOR_GREEN STYLE_BOLD "[ MANAGER ]" COLOR_RESET
               COLOR_WHITE "  User: " STYLE_BOLD "%s" COLOR_RESET "\n\n", user);
    } else {
        printf("  " COLOR_YELLOW STYLE_BOLD "[ INSPECTOR ]" COLOR_RESET
               COLOR_WHITE "  User: " STYLE_BOLD "%s" COLOR_RESET "\n\n", user);
    }
}

/* Repeated character helper */
static void repeat_char(const char *ch, int n) {
    for (int i = 0; i < n; i++) printf("%s", ch);
}

void print_box_top(int width) {
    printf(COLOR_CYAN BOX_TL);
    repeat_char(BOX_H, width);
    printf(BOX_TR COLOR_RESET "\n");
}

void print_box_bot(int width) {
    printf(COLOR_CYAN BOX_BL);
    repeat_char(BOX_H, width);
    printf(BOX_BR COLOR_RESET "\n");
}

void print_box_mid(int width) {
    printf(COLOR_CYAN BOX_ML);
    repeat_char(BOX_H, width);
    printf(BOX_MR COLOR_RESET "\n");
}

/* Print one labelled row inside a box */
void print_box_row(const char *label, const char *value, int width) {
    // width = total inner width (label col + value col)
    // label col = 14 chars, value fills the rest
    int val_width = width - 16; // 2 spaces padding each side + 14 label
    printf(COLOR_CYAN BOX_V COLOR_RESET
           " " STYLE_BOLD COLOR_WHITE "%-14s" COLOR_RESET
           " " COLOR_CYAN BOX_V COLOR_RESET
           " %-*s " COLOR_CYAN BOX_V COLOR_RESET "\n",
           label, val_width, value);
}

/* Progress bar replacing the old spinner */
void progress_bar(const char *label, int steps, int delay_us) {
    int bar_width = 30;
    printf("\n");
    for (int i = 0; i <= steps; i++) {
        int filled = (i * bar_width) / steps;
        printf("\r  " COLOR_CYAN "%s" COLOR_RESET " [", label);
        printf(COLOR_GREEN);
        for (int j = 0; j < filled; j++)    printf("█");
        printf(COLOR_RESET COLOR_CYAN STYLE_DIM);
        for (int j = filled; j < bar_width; j++) printf("░");
        printf(COLOR_RESET "] " STYLE_BOLD "%3d%%" COLOR_RESET, (i * 100) / steps);
        fflush(stdout);
        if (i < steps) usleep(delay_us);
    }
    printf("\n\n");
}

// ============================================================
//  CORE HELPERS
// ============================================================

void get_permission_string(mode_t mode, char *str) {
    strcpy(str, "---------");
    if (mode & S_IRUSR) str[0] = 'r';
    if (mode & S_IWUSR) str[1] = 'w';
    if (mode & S_IXUSR) str[2] = 'x';
    if (mode & S_IRGRP) str[3] = 'r';
    if (mode & S_IWGRP) str[4] = 'w';
    if (mode & S_IXGRP) str[5] = 'x';
    if (mode & S_IROTH) str[6] = 'r';
    if (mode & S_IWOTH) str[7] = 'w';
    if (mode & S_IXOTH) str[8] = 'x';
}

int check_role_permission(const char *filepath, const char *role, int need_write) {
    struct stat st;
    if (stat(filepath, &st) == -1) return 1;
    mode_t mode = st.st_mode;
    int allowed = 0;
    if (strcmp(role, "manager") == 0)
        allowed = need_write ? (mode & S_IWUSR) : (mode & S_IRUSR);
    else
        allowed = need_write ? (mode & S_IWGRP) : (mode & S_IRGRP);

    if (!allowed) {
        char perms[10];
        get_permission_string(mode, perms);
        printf("\n  " COLOR_RED "╳  PERMISSION DENIED" COLOR_RESET
               "  Role '%s' lacks %s on '%s' (perms: %s)\n\n",
               role, need_write ? "write" : "read", filepath, perms);
    }
    return allowed != 0;
}

void log_action(const char *district_id, const char *role, const char *user, const char *action) {
    char log_path[2048];
    snprintf(log_path, sizeof(log_path), "%s/logged_district", district_id);

    struct stat st;
    if (stat(log_path, &st) == 0 && strcmp(role, "inspector") == 0) {
        printf(COLOR_YELLOW "  ⚠  Inspector cannot write to logged_district (644). Log skipped.\n" COLOR_RESET);
        return;
    }

    FILE *log_file = fopen(log_path, "a");
    if (log_file == NULL) return;
    time_t now = time(NULL);
    char time_buf[64];
    strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", localtime(&now));
    fprintf(log_file, "%s\t%s\t%s\t%s\n", time_buf, user, role, action);
    fclose(log_file);
}

void create_symlink(const char *district_id) {
    char link_name[1024];
    char target[2048];
    snprintf(link_name, sizeof(link_name), "active_reports-%s", district_id);
    snprintf(target,    sizeof(target),    "%s/reports.dat",    district_id);

    struct stat lst;
    if (lstat(link_name, &lst) == 0) {
        if (S_ISLNK(lst.st_mode)) {
            struct stat tst;
            if (stat(link_name, &tst) == -1)
                printf(COLOR_YELLOW "  ⚠  Dangling symlink: %s — Recreating.\n" COLOR_RESET, link_name);
            unlink(link_name);
        }
    }
    if (symlink(target, link_name) == 0)
        printf("  " COLOR_GREEN "⇒" COLOR_RESET "  Symlink: " STYLE_BOLD "%s" COLOR_RESET " → %s\n", link_name, target);
    else if (errno != EEXIST)
        perror("  symlink");
}

// ============================================================
//  DIRECTORY SETUP
// ============================================================

void make_directory_with_cfg(const char* filepath, const char* dir_name) {
    char dir_path[1024], cfg_path[2048], log_path[2048];
    snprintf(dir_path, sizeof(dir_path), "%s/%s", filepath, dir_name);

    if (mkdir(dir_path, 0750) == 0) {
        chmod(dir_path, 0750);
        printf("  " COLOR_GREEN "✓" COLOR_RESET "  District dir: " STYLE_BOLD "%s" COLOR_RESET " (750)\n", dir_path);

        snprintf(cfg_path, sizeof(cfg_path), "%s/district.cfg", dir_path);
        FILE *cfg_file = fopen(cfg_path, "w");
        if (cfg_file) {
            fprintf(cfg_file, "severity_threshold=2\n");
            fclose(cfg_file);
            chmod(cfg_path, 0640);
            printf("  " COLOR_GREEN "✓" COLOR_RESET "  Config:       %s (640)\n", cfg_path);
        } else perror("cfg");

        snprintf(log_path, sizeof(log_path), "%s/logged_district", dir_path);
        FILE *log_file = fopen(log_path, "w");
        if (log_file) {
            fclose(log_file);
            chmod(log_path, 0644);
            printf("  " COLOR_GREEN "✓" COLOR_RESET "  Log file:     %s (644)\n", log_path);
        } else perror("log");
    } else {
        perror("mkdir");
    }
}

// ============================================================
//  MONITOR NOTIFICATION (Phase 2)
// ============================================================

static void notify_monitor(const char *district_id, const char *role, const char *user) {
    int fd = open(".monitor_pid", O_RDONLY);
    if (fd == -1) {
        log_action(district_id, role, user, "add: monitor not notified (no .monitor_pid file)");
        return;
    }
    char buf[32];
    ssize_t n = read(fd, buf, sizeof(buf) - 1);
    close(fd);
    if (n <= 0) {
        log_action(district_id, role, user, "add: monitor not notified (unreadable .monitor_pid)");
        return;
    }
    buf[n] = '\0';
    pid_t mon_pid = (pid_t)atoi(buf);

    char msg[256];
    if (kill(mon_pid, SIGUSR1) == 0) {
        snprintf(msg, sizeof(msg), "add: monitor PID %d notified via SIGUSR1", (int)mon_pid);
        printf("  " COLOR_GREEN "⚑" COLOR_RESET "  Monitor (PID %d) notified.\n", (int)mon_pid);
    } else {
        snprintf(msg, sizeof(msg), "add: could not notify monitor PID %d (%s)",
                 (int)mon_pid, strerror(errno));
        printf("  " COLOR_YELLOW "⚠  Could not notify monitor: %s\n" COLOR_RESET, strerror(errno));
    }
    log_action(district_id, role, user, msg);
}

// ============================================================
//  ADD
// ============================================================

void add_record(const char* filename, Record* record) {
    int fd = open(filename, O_WRONLY | O_APPEND | O_CREAT, 0664);
    if (fd == -1) {
        printf(COLOR_RED "  ✗  Cannot open %s for writing\n" COLOR_RESET, filename);
        return;
    }
    write(fd, record, sizeof(Record));
    close(fd);
}

Record add_record_file(const char *user) {
    Record new_record;
    memset(&new_record, 0, sizeof(Record));
    new_record.id = rand();

    strncpy(new_record.inspector_name, user, sizeof(new_record.inspector_name) - 1);
    new_record.inspector_name[sizeof(new_record.inspector_name) - 1] = '\0';

    printf("  " COLOR_CYAN "Longitude" COLOR_RESET "  ► ");
    scanf("%f", &new_record.longitude);

    printf("  " COLOR_CYAN "Latitude " COLOR_RESET "  ► ");
    scanf("%f", &new_record.latitude);

    printf("  " COLOR_CYAN "Category " COLOR_RESET "  ► (road/lighting/flooding/other): ");
    scanf("%49s", new_record.issue_category);

    int security_level;
    while (1) {
        printf("  " COLOR_CYAN "Severity " COLOR_RESET "  ► (1=minor / 2=moderate / 3=critical): ");
        if (scanf("%d", &security_level) == 1 && security_level >= 1 && security_level <= 3) {
            new_record.security_level = security_level;
            break;
        }
        // Clear whatever garbage is left in the input buffer
        int ch; while ((ch = getchar()) != '\n' && ch != EOF);
        printf("  " COLOR_YELLOW "⚠  Must be 1, 2 or 3.\n" COLOR_RESET);
    }

    printf("  " COLOR_CYAN "Description" COLOR_RESET " ► ");
    int c; while ((c = getchar()) != '\n' && c != EOF);
    fgets(new_record.description, sizeof(new_record.description), stdin);
    new_record.description[strcspn(new_record.description, "\n")] = '\0';

    new_record.timestamp = (size_t)time(NULL);
    return new_record;
}

void handle_add(const char *district_id, const char *role, const char *user) {
    print_banner(role, user);
    printf(COLOR_CYAN STYLE_BOLD "  ╔═══ ADD REPORT ═════════════════════════╗\n" COLOR_RESET);
    printf(COLOR_CYAN            "  ║  District: %-26s  ║\n" COLOR_RESET, district_id);
    printf(COLOR_CYAN STYLE_BOLD "  ╚════════════════════════════════════════╝\n\n" COLOR_RESET);

    progress_bar("Preparing district", 20, 40000);

    struct stat st = {0};
    char file_path[2048];

    if (stat(district_id, &st) == -1) {
        printf(COLOR_YELLOW "  ⚠  District '%s' not found — creating...\n\n" COLOR_RESET, district_id);
        make_directory_with_cfg(".", district_id);
    } else {
        printf("  " COLOR_GREEN "✓" COLOR_RESET "  District found: " STYLE_BOLD "%s\n\n" COLOR_RESET, district_id);
    }

    snprintf(file_path, sizeof(file_path), "%s/reports.dat", district_id);
    int fd = open(file_path, O_CREAT | O_WRONLY | O_APPEND, 0664);
    if (fd == -1) { printf(COLOR_RED "  ✗  Failed to create %s\n" COLOR_RESET, file_path); return; }
    close(fd);
    chmod(file_path, 0664);

    if (!check_role_permission(file_path, role, 1)) return;

    printf(COLOR_CYAN STYLE_BOLD "  ┌─ Enter Report Data ─────────────────┐\n\n" COLOR_RESET);
    Record new_record = add_record_file(user);
    printf(COLOR_CYAN STYLE_BOLD "\n  └─────────────────────────────────────┘\n\n" COLOR_RESET);

    add_record(file_path, &new_record);
    create_symlink(district_id);

    char action_buf[128];
    snprintf(action_buf, sizeof(action_buf), "add report_id=%d", new_record.id);
    log_action(district_id, role, user, action_buf);

    notify_monitor(district_id, role, user);

    printf("  " COLOR_GREEN STYLE_BOLD "✓  Report saved!" COLOR_RESET
           "  ID: " STYLE_BOLD COLOR_YELLOW "%d" COLOR_RESET "\n\n", new_record.id);
}

// ============================================================
//  REMOVE Direcortory (by ID, with compaction)
// ============================================================

void handle_remove_district(const char *district_id, const char *role, const char *user) {
    (void)user;
    if (strcmp(role, "manager") != 0) {
        printf(COLOR_RED "  ╳  Only managers may remove districts.\n\n" COLOR_RESET);
        return;
    }
    if (!check_role_permission(district_id, role, 1)) return;

    char link_name[256];
    snprintf(link_name, sizeof(link_name), "active_reports-%s", district_id);

    pid_t pid = fork();
    if (pid == -1) {
        perror("fork");
        return;
    }
    if (pid == 0) {
        /* child: delete district directory and its symlink */
        execlp("rm", "rm", "-rf", district_id, link_name, (char *)NULL);
        perror("execlp rm");
        _exit(1);
    }

    int status;
    waitpid(pid, &status, 0);
    if (WIFEXITED(status) && WEXITSTATUS(status) == 0)
        printf("  " COLOR_GREEN STYLE_BOLD "✓  District '%s' removed.\n\n" COLOR_RESET, district_id);
    else
        printf(COLOR_RED "  ✗  Failed to remove district '%s'.\n\n" COLOR_RESET, district_id);
}

// ============================================================
//  REMOVE REPORT (by ID, Phase 1)
// ============================================================

void handle_remove_report(const char *district_id, const char *report_id,
                          const char *role, const char *user) {
    print_banner(role, user);

    if (strcmp(role, "manager") != 0) {
        printf(COLOR_RED "  ╳  Only managers may remove reports.\n\n" COLOR_RESET);
        return;
    }

    char file_path[2048];
    snprintf(file_path, sizeof(file_path), "%s/reports.dat", district_id);
    if (!check_role_permission(file_path, role, 1)) return;

    int target_id = atoi(report_id);
    int fd = open(file_path, O_RDWR);
    if (fd == -1) {
        printf(COLOR_RED "  ✗  Cannot open %s\n\n" COLOR_RESET, file_path);
        return;
    }

    /* Locate the record to remove */
    Record r;
    off_t remove_pos = -1;
    off_t cur = 0;
    while (read(fd, &r, sizeof(Record)) == (ssize_t)sizeof(Record)) {
        if (r.id == target_id) { remove_pos = cur; break; }
        cur += (off_t)sizeof(Record);
    }

    if (remove_pos == -1) {
        printf(COLOR_YELLOW "  ⚠  Report ID %d not found in '%s'.\n\n" COLOR_RESET,
               target_id, district_id);
        close(fd);
        return;
    }

    /* Shift every subsequent record one slot earlier using lseek */
    off_t rpos = remove_pos + (off_t)sizeof(Record);
    off_t wpos = remove_pos;
    while (1) {
        lseek(fd, rpos, SEEK_SET);
        ssize_t n = read(fd, &r, sizeof(Record));
        if (n != (ssize_t)sizeof(Record)) break;
        lseek(fd, wpos, SEEK_SET);
        write(fd, &r, sizeof(Record));
        rpos += (off_t)sizeof(Record);
        wpos += (off_t)sizeof(Record);
    }

    /* Shrink the file by one record */
    struct stat st;
    fstat(fd, &st);
    ftruncate(fd, st.st_size - (off_t)sizeof(Record));
    close(fd);

    printf("  " COLOR_GREEN STYLE_BOLD "✓  Report %d removed from '%s'.\n\n" COLOR_RESET,
           target_id, district_id);

    char action_buf[128];
    snprintf(action_buf, sizeof(action_buf), "remove_report report_id=%d", target_id);
    log_action(district_id, role, user, action_buf);
}

// ============================================================
//  LIST  (table layout)
// ============================================================

    void handle_list(const char *district_id, const char *role, const char *user) {
        print_banner(role, user);

        char file_path[2048];
        snprintf(file_path, sizeof(file_path), "%s/reports.dat", district_id);
        if (!check_role_permission(file_path, role, 0)) return;

        struct stat file_stat;
        if (stat(file_path, &file_stat) == -1) {
            printf(COLOR_RED "  ✗  Cannot access reports for '%s'.\n\n" COLOR_RESET, district_id);
            return;
        }

    // Symlink check via lstat
    char link_name[1024];
    snprintf(link_name, sizeof(link_name), "active_reports-%s", district_id);
    struct stat lst;
    if (lstat(link_name, &lst) == 0 && S_ISLNK(lst.st_mode)) {
        struct stat tst;
        if (stat(link_name, &tst) == -1)
            printf(COLOR_YELLOW "  ⚠  Dangling symlink: %s\n" COLOR_RESET, link_name);
    }

    char perms[10];
    get_permission_string(file_stat.st_mode, perms);
    char time_buf[64];
    strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", localtime(&file_stat.st_mtime));

    // File info box
    int W = 52;
    printf(COLOR_CYAN STYLE_BOLD);
    printf("  ╔"); repeat_char("═", W); printf("╗\n");
    printf("  ║  %-*s║\n", W-1, " FILE INFORMATION");
    printf("  ╠"); repeat_char("═", W); printf("╣\n");
    printf(COLOR_RESET);

    char val[2200];
    snprintf(val, sizeof(val), "%s", file_path);
    printf("  " COLOR_CYAN "║" COLOR_RESET " " STYLE_BOLD "%-12s" COLOR_RESET " " COLOR_CYAN "│" COLOR_RESET " %-*s " COLOR_CYAN "║" COLOR_RESET "\n", "Path", W-17, val);
    printf("  " COLOR_CYAN "║" COLOR_RESET " " STYLE_BOLD "%-12s" COLOR_RESET " " COLOR_CYAN "│" COLOR_RESET " %-*s " COLOR_CYAN "║" COLOR_RESET "\n", "Permissions", W-17, perms);
    snprintf(val, sizeof(val), "%lld bytes", (long long)file_stat.st_size);
    printf("  " COLOR_CYAN "║" COLOR_RESET " " STYLE_BOLD "%-12s" COLOR_RESET " " COLOR_CYAN "│" COLOR_RESET " %-*s " COLOR_CYAN "║" COLOR_RESET "\n", "Size", W-17, val);
    printf("  " COLOR_CYAN "║" COLOR_RESET " " STYLE_BOLD "%-12s" COLOR_RESET " " COLOR_CYAN "│" COLOR_RESET " %-*s " COLOR_CYAN "║" COLOR_RESET "\n", "Last Mod", W-17, time_buf);

    printf(COLOR_CYAN STYLE_BOLD);
    printf("  ╠"); repeat_char("═", W); printf("╣\n");
    printf("  ║  %-*s║\n", W-1, " REPORTS");
    printf("  ╠═══════════╦"); repeat_char("═", W-14); printf("╣\n");
    printf("  ║ " STYLE_BOLD "%-9s" COLOR_RESET COLOR_CYAN " ║" COLOR_RESET " %-*s " COLOR_CYAN "║\n" COLOR_RESET, " ID", W-17, "Category         Sev  Inspector");
    printf(COLOR_CYAN "  ╠═══════════╬"); repeat_char("═", W-14); printf("╣\n" COLOR_RESET);

    int fd = open(file_path, O_RDONLY);
    if (fd == -1) {
        printf(COLOR_RED "  ✗  Cannot open file.\n\n" COLOR_RESET); return;
    }

    Record r;
    int count = 0;
    while (read(fd, &r, sizeof(Record)) == sizeof(Record)) {
        count++;
        // severity color
        const char *sev_color = r.security_level == 3 ? COLOR_RED :
                                r.security_level == 2 ? COLOR_YELLOW : COLOR_GREEN;
        char row[128];
        snprintf(row, sizeof(row), "%-16s %s%d" COLOR_RESET "    %s",
                 r.issue_category, sev_color, r.security_level, r.inspector_name);
        printf("  " COLOR_CYAN "║" COLOR_RESET " " COLOR_YELLOW STYLE_BOLD "%9d" COLOR_RESET
               " " COLOR_CYAN "║" COLOR_RESET " %-*s " COLOR_CYAN "║\n" COLOR_RESET,
               r.id, (int)(W-17+strlen(sev_color)+strlen(COLOR_RESET)), row);
    }
    close(fd);

    printf(COLOR_CYAN "  ╚═══════════╩"); repeat_char("═", W-14); printf("╝\n\n" COLOR_RESET);

    if (count == 0)
        printf("  " COLOR_YELLOW "No reports found.\n\n" COLOR_RESET);
    else
        printf("  " COLOR_GREEN STYLE_BOLD "✓  %d report(s) listed.\n\n" COLOR_RESET, count);

    log_action(district_id, role, user, "list");
}

// ============================================================
//  VIEW
// ============================================================

void handle_view(const char *district_id, const char *report_id, const char *role, const char *user) {
    print_banner(role, user);

    char file_path[2048];
    snprintf(file_path, sizeof(file_path), "%s/reports.dat", district_id);
    if (!check_role_permission(file_path, role, 0)) return;

    int target_id = atoi(report_id);
    int fd = open(file_path, O_RDONLY);
    if (fd == -1) { printf(COLOR_RED "  ✗  Cannot open %s\n\n" COLOR_RESET, file_path); return; }

    Record r;
    int found = 0;
    while (read(fd, &r, sizeof(Record)) == sizeof(Record)) {
        if (r.id == target_id) {
            found = 1;
            char record_time[64];
            time_t ts = (time_t)r.timestamp;
            strftime(record_time, sizeof(record_time), "%Y-%m-%d %H:%M:%S", localtime(&ts));

            const char *sev_color = r.security_level == 3 ? COLOR_RED :
                                    r.security_level == 2 ? COLOR_YELLOW : COLOR_GREEN;
            const char *sev_label = r.security_level == 3 ? "CRITICAL" :
                                    r.security_level == 2 ? "MODERATE" : "MINOR";

            int W = 48;
            printf(COLOR_CYAN STYLE_BOLD "  ╔"); repeat_char("═", W); printf("╗\n");
            printf("  ║  REPORT #%-*d║\n", W-9, r.id);
            printf("  ╠"); repeat_char("═", W); printf("╣\n" COLOR_RESET);

            char val[256];
            #define ROW(lbl, v) \
                printf("  " COLOR_CYAN "║" COLOR_RESET " " STYLE_BOLD "%-12s" COLOR_RESET \
                       " " COLOR_CYAN "│" COLOR_RESET " %-*s " COLOR_CYAN "║" COLOR_RESET "\n", lbl, W-17, v)

            ROW("Inspector",  r.inspector_name[0] ? r.inspector_name : "N/A");
            ROW("Category",   r.issue_category);
            snprintf(val, sizeof(val), "%s%d — %s" COLOR_RESET, sev_color, r.security_level, sev_label);
            printf("  " COLOR_CYAN "║" COLOR_RESET " " STYLE_BOLD "%-12s" COLOR_RESET
                   " " COLOR_CYAN "│" COLOR_RESET " %-*s " COLOR_CYAN "║" COLOR_RESET "\n",
                   "Severity", W - 17 + (int)(strlen(sev_color) + strlen(COLOR_RESET)), val);
            snprintf(val, sizeof(val), "Lat %.4f  Lon %.4f", r.latitude, r.longitude);
            ROW("Location", val);
            ROW("Logged At",  record_time);
            ROW("Description", r.description[0] ? r.description : "N/A");
            #undef ROW

            printf(COLOR_CYAN STYLE_BOLD "  ╚"); repeat_char("═", W); printf("╝\n\n" COLOR_RESET);
            break;
        }
    }
    close(fd);

    if (!found)
        printf(COLOR_YELLOW "  ⚠  Report ID %d not found in '%s'.\n\n" COLOR_RESET, target_id, district_id);

    log_action(district_id, role, user, "view");
}

// ============================================================
//  UPDATE THRESHOLD
// ============================================================

void handle_update_threshold(const char *district_id, const char *value, const char *role, const char *user) {
    print_banner(role, user);

    if (strcmp(role, "manager") != 0) {
        printf(COLOR_RED "  ╳  Only managers may update the threshold.\n\n" COLOR_RESET);
        return;
    }

    char cfg_path[2048];
    snprintf(cfg_path, sizeof(cfg_path), "%s/district.cfg", district_id);

    struct stat st;
    if (stat(cfg_path, &st) == -1) {
        printf(COLOR_RED "  ✗  Cannot stat %s.\n\n" COLOR_RESET, cfg_path);
        return;
    }

    mode_t perms = st.st_mode & 0777;
    if (perms != 0640) {
        char ps[10]; get_permission_string(st.st_mode, ps);
        printf(COLOR_RED "  ✗  district.cfg permissions altered (expected 640, got %s). Refusing.\n\n" COLOR_RESET, ps);
        return;
    }
    if (!check_role_permission(cfg_path, role, 1)) return;

    progress_bar("Writing config", 10, 30000);

    FILE *cfg = fopen(cfg_path, "w");
    if (!cfg) { perror("fopen"); return; }
    fprintf(cfg, "severity_threshold=%s\n", value);
    fclose(cfg);
    chmod(cfg_path, 0640);

    printf("  " COLOR_GREEN STYLE_BOLD "✓  Threshold updated to %s" COLOR_RESET " in %s\n\n", value, cfg_path);

    char action_buf[128];
    snprintf(action_buf, sizeof(action_buf), "update_threshold value=%s", value);
    log_action(district_id, role, user, action_buf);
}

// ============================================================
//  FILTER
// ============================================================

int parse_condition(const char *input, char *field, char *op, char *value) {
    char buf[256];
    strncpy(buf, input, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';
    char *fc = strchr(buf, ':');
    if (!fc) return 0;
    *fc = '\0';
    strcpy(field, buf);
    char *sc = strchr(fc + 1, ':');
    if (!sc) return 0;
    *sc = '\0';
    strcpy(op, fc + 1);
    strcpy(value, sc + 1);
    return 1;
}

int match_condition(Record *r, const char *field, const char *op, const char *value) {
    if (strcmp(field, "severity") == 0) {
        int rv = r->security_level, cv = atoi(value);
        if (strcmp(op,"==") == 0) return rv == cv;
        if (strcmp(op,"!=") == 0) return rv != cv;
        if (strcmp(op,"<")  == 0) return rv <  cv;
        if (strcmp(op,"<=") == 0) return rv <= cv;
        if (strcmp(op,">")  == 0) return rv >  cv;
        if (strcmp(op,">=") == 0) return rv >= cv;
    } else if (strcmp(field, "timestamp") == 0) {
        size_t rv = r->timestamp, cv = (size_t)atoll(value);
        if (strcmp(op,"==") == 0) return rv == cv;
        if (strcmp(op,"!=") == 0) return rv != cv;
        if (strcmp(op,"<")  == 0) return rv <  cv;
        if (strcmp(op,"<=") == 0) return rv <= cv;
        if (strcmp(op,">")  == 0) return rv >  cv;
        if (strcmp(op,">=") == 0) return rv >= cv;
    } else if (strcmp(field, "category") == 0) {
        int c = strcmp(r->issue_category, value);
        if (strcmp(op,"==") == 0) return c == 0;
        if (strcmp(op,"!=") == 0) return c != 0;
    } else if (strcmp(field, "inspector") == 0) {
        int c = strcmp(r->inspector_name, value);
        if (strcmp(op,"==") == 0) return c == 0;
        if (strcmp(op,"!=") == 0) return c != 0;
    }
    return 0;
}

void handle_filter(const char *district_id, int num_conditions, char **conditions, const char *role, const char *user) {
    print_banner(role, user);

    char file_path[2048];
    snprintf(file_path, sizeof(file_path), "%s/reports.dat", district_id);
    if (!check_role_permission(file_path, role, 0)) return;

    char fields[10][64], ops[10][8], values[10][128];
    for (int i = 0; i < num_conditions; i++) {
        if (!parse_condition(conditions[i], fields[i], ops[i], values[i])) {
            printf(COLOR_RED "  ✗  Bad condition '%s'. Use field:op:value.\n\n" COLOR_RESET, conditions[i]);
            return;
        }
    }

    printf(COLOR_CYAN STYLE_BOLD "  Filter: " COLOR_RESET);
    for (int i = 0; i < num_conditions; i++)
        printf(COLOR_YELLOW "%s" COLOR_RESET "%s", conditions[i], i < num_conditions-1 ? "  AND  " : "");
    printf("\n\n");

    progress_bar("Scanning records", 12, 25000);

    int fd = open(file_path, O_RDONLY);
    if (fd == -1) { printf(COLOR_RED "  ✗  Cannot open %s\n\n" COLOR_RESET, file_path); return; }

    Record r;
    int match_count = 0;

    while (read(fd, &r, sizeof(Record)) == sizeof(Record)) {
        int all_match = 1;
        for (int i = 0; i < num_conditions; i++)
            if (!match_condition(&r, fields[i], ops[i], values[i])) { all_match = 0; break; }

        if (all_match) {
            match_count++;
            char record_time[64];
            time_t ts = (time_t)r.timestamp;
            strftime(record_time, sizeof(record_time), "%Y-%m-%d %H:%M:%S", localtime(&ts));

            const char *sc = r.security_level == 3 ? COLOR_RED :
                             r.security_level == 2 ? COLOR_YELLOW : COLOR_GREEN;

            int W = 46;
            printf(COLOR_CYAN "  ┌"); repeat_char("─", W); printf("┐\n");
            printf("  │ " COLOR_YELLOW STYLE_BOLD "#%-*d" COLOR_RESET COLOR_CYAN "│\n" COLOR_RESET, W-2, r.id);
            printf(COLOR_CYAN "  ├"); repeat_char("─", W); printf("┤\n" COLOR_RESET);

            char val[256];
            #define FROW(lbl, v) \
                printf("  " COLOR_CYAN "│" COLOR_RESET " " STYLE_BOLD "%-11s" COLOR_RESET \
                       " %-*s " COLOR_CYAN "│\n" COLOR_RESET, lbl, W-14, v)

            FROW("Inspector:", r.inspector_name[0] ? r.inspector_name : "N/A");
            FROW("Category:", r.issue_category);
            snprintf(val, sizeof(val), "%s%d" COLOR_RESET, sc, r.security_level);
            printf("  " COLOR_CYAN "│" COLOR_RESET " " STYLE_BOLD "%-11s" COLOR_RESET
                   " %-*s " COLOR_CYAN "│\n" COLOR_RESET,
                   "Severity:", W - 14 + (int)(strlen(sc) + strlen(COLOR_RESET)), val);
            snprintf(val, sizeof(val), "Lat %.4f  Lon %.4f", r.latitude, r.longitude);
            FROW("Location:", val);
            FROW("Time:", record_time);
            FROW("Desc:", r.description[0] ? r.description : "N/A");
            #undef FROW
            printf(COLOR_CYAN "  └"); repeat_char("─", W); printf("┘\n\n" COLOR_RESET);
        }
    }
    close(fd);

    if (match_count == 0)
        printf("  " COLOR_YELLOW "No records matched.\n\n" COLOR_RESET);
    else
        printf("  " COLOR_GREEN STYLE_BOLD "✓  %d record(s) matched.\n\n" COLOR_RESET, match_count);

    log_action(district_id, role, user, "filter");
}

// ============================================================
//  HELP
// ============================================================

void handle_help(void) {
    printf("\n");
    // Top border (75 '=' characters)
    printf(COLOR_CYAN STYLE_BOLD "  ╔═══════════════════════════════════════════════════════════════════════════╗\n");
    
    // Title row (26 spaces + 23 text chars + 26 spaces = 75 total inner width)
    printf("  ║                          CITY MANAGER - COMMANDS                          ║\n");
    
    // Middle separator
    printf("  ╠═══════════════════════════════════════════════════════════════════════════╣\n" COLOR_RESET);

    const char *cmds[][2] = {
        {"--add <district>",               "Append a new report"},
        {"--list <district>",              "List all reports + file info"},
        {"--view <district> <id>",         "Print full report details"},
        {"--remove_report <district> <id>","Remove a report by ID (manager only)"},
        {"--remove_district <district>",   "Remove entire district (manager only)"},
        {"--update_threshold <d> <val>",   "Update severity threshold (manager)"},
        {"--filter <district> <cond...>",  "Filter: field:op:value"},
        {"--help",                         "Show this menu"},
        {NULL, NULL}
    };

    for (int i = 0; cmds[i][0]; i++) {
        // Inner width calculation: 
        // 1 space + 34 chars (left) + 1 space + 38 chars (right) + 1 space = 75 total width
        printf("  " COLOR_CYAN "║" COLOR_RESET " " COLOR_YELLOW STYLE_BOLD "%-34s" COLOR_RESET
               " %-38s " COLOR_CYAN "║\n" COLOR_RESET, cmds[i][0], cmds[i][1]);
    }
    
    // Bottom border
    printf(COLOR_CYAN STYLE_BOLD "  ╚═══════════════════════════════════════════════════════════════════════════╝\n\n" COLOR_RESET);
    
    printf("  Fields for filter: " STYLE_BOLD "severity, category, inspector, timestamp\n" COLOR_RESET);
    printf("  Operators:         " STYLE_BOLD "==  !=  <  <=  >  >=\n\n" COLOR_RESET);
}