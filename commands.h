#ifndef MY_UTILS_H
#define MY_UTILS_H

#include <stddef.h>
#include <sys/stat.h>

// --- Struct Definitions ---
typedef struct {
    int id;
    char inspector_name[50];
    float latitude;
    float longitude;
    char issue_category[50];
    int security_level;
    size_t timestamp;
    char description[256];
} Record;

// --- Function Prototypes ---
void make_directory_with_cfg(const char* filepath, const char* dir_name);
void handle_add(const char *district_id, const char *role, const char *user);
void handle_remove_district(const char *district_id, const char *role, const char *user);
void handle_remove_report(const char *district_id, const char *report_id, const char *role, const char *user);
void handle_list(const char *district_id, const char *role, const char *user);
void handle_view(const char *district_id, const char *report_id, const char *role, const char *user);
void handle_update_threshold(const char *district_id, const char *value, const char *role, const char *user);
void handle_filter(const char *district_id, int num_conditions, char **conditions, const char *role, const char *user);
void handle_help(void);
void progress_bar(const char *label, int steps, int delay_us);
Record add_record_file(const char *user);
void add_record(const char* filename, Record* record);
void get_permission_string(mode_t mode, char *str);
void log_action(const char *district_id, const char *role, const char *user, const char *action);
void create_symlink(const char *district_id);
int check_role_permission(const char *filepath, const char *role, int need_write);
void print_banner(const char *role, const char *user);
void print_box_top(int width);
void print_box_mid(int width);
void print_box_bot(int width);
void print_box_row(const char *label, const char *value, int width);

// AI-assisted filter helpers
int parse_condition(const char *input, char *field, char *op, char *value);
int match_condition(Record *r, const char *field, const char *op, const char *value);

// --- Color Macros ---
#define COLOR_RESET   "\033[0m"
#define COLOR_RED     "\033[31m"
#define COLOR_GREEN   "\033[32m"
#define COLOR_YELLOW  "\033[33m"
#define COLOR_BLUE    "\033[34m"
#define COLOR_MAGENTA "\033[35m"
#define COLOR_CYAN    "\033[36m"
#define COLOR_WHITE   "\033[37m"
#define BG_BLUE       "\033[44m"
#define BG_DARK       "\033[40m"

// --- Text Style Macros ---
#define STYLE_BOLD    "\033[1m"
#define STYLE_DIM     "\033[2m"
#define STYLE_RESET   "\033[0m"

// --- Box drawing (UTF-8) ---
#define BOX_TL  "╔"
#define BOX_TR  "╗"
#define BOX_BL  "╚"
#define BOX_BR  "╝"
#define BOX_H   "═"
#define BOX_V   "║"
#define BOX_ML  "╠"
#define BOX_MR  "╣"
#define BOX_MT  "╦"
#define BOX_MB  "╩"

#endif