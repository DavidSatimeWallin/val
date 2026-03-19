/* diff.c, Git diff functionality, Val Emacs */

#include "header.h"
#include <stdlib.h>

#define DIFF_BUF     65536
#define MAX_DIFF_LINES 5000

typedef struct {
    int offset;
    int len;
    int file_line;
    int is_hunk_header;
    int line_type;
} diff_line_t;

enum {
    LINE_NORMAL = 0,
    LINE_ADD = 1,
    LINE_DEL = 2,
    LINE_HUNK = 3,
    LINE_HEADER = 4
};

static char diff_output[DIFF_BUF];
static diff_line_t diff_lines[MAX_DIFF_LINES];
static int diff_nlines;
static int cursor_pos;
static int view_start;

static void init_colors(void)
{
    init_pair(1, COLOR_RED, COLOR_BLACK);
    init_pair(2, COLOR_GREEN, COLOR_BLACK);
    init_pair(3, COLOR_CYAN, COLOR_BLACK);
    init_pair(4, COLOR_YELLOW, COLOR_BLACK);
    init_pair(5, COLOR_BLACK, COLOR_YELLOW);
}

static void draw_diff_view(void)
{
    erase();
    init_colors();

    attron(A_REVERSE);
    mvaddstr(0, 0, " Git Diff | Esc/C-g=close | Enter=go to line");
    clrtoeol();
    attroff(A_REVERSE);

    int display_rows = LINES - 2;

    for (int vis_row = 1; vis_row < display_rows; vis_row++) {
        int line_idx = view_start + vis_row - 1;
        if (line_idx < 0 || line_idx >= diff_nlines) continue;

        char *line_start = diff_output + diff_lines[line_idx].offset;
        int len = diff_lines[line_idx].len;
        int is_selected = (line_idx == cursor_pos);

        if (is_selected) {
            attrset(COLOR_PAIR(5) | A_BOLD);
            move(vis_row, 0);
            clrtoeol();
            mvaddnstr(vis_row, 0, line_start, len);
            clrtoeol();
            attroff(COLOR_PAIR(5));
            attroff(A_BOLD);
        } else {
            int pair = 0;
            switch (diff_lines[line_idx].line_type) {
                case LINE_ADD: pair = 2; break;
                case LINE_DEL: pair = 1; break;
                case LINE_HUNK: pair = 3; break;
                case LINE_HEADER: pair = 4; break;
            }
            if (pair > 0) {
                attrset(COLOR_PAIR(pair));
                mvaddnstr(vis_row, 0, line_start, len);
                clrtoeol();
                attroff(COLOR_PAIR(pair));
            } else {
                mvaddnstr(vis_row, 0, line_start, len);
                clrtoeol();
            }
        }
    }

    move(LINES - 1, 0);
    clrtoeol();
    attron(A_REVERSE);
    mvaddstr(LINES - 1, 0, " Up/Down=move | Enter=go to line | q/Esc=close");
    clrtoeol();
    attroff(A_REVERSE);
    refresh();
}

static void parse_diff_lines(void)
{
    diff_nlines = 0;
    int offset = 0;

    while (diff_output[offset] && offset < DIFF_BUF) {
        int line_start = offset;
        while (diff_output[offset] && diff_output[offset] != '\n') offset++;
        int len = offset - line_start;
        if (len > 0) {
            diff_lines[diff_nlines].offset = line_start;
            diff_lines[diff_nlines].len = len;
            diff_lines[diff_nlines].file_line = 0;
            diff_lines[diff_nlines].is_hunk_header = 0;
            diff_lines[diff_nlines].line_type = LINE_NORMAL;

            char *line = diff_output + line_start;
            if (line[0] == '@') {
                diff_lines[diff_nlines].line_type = LINE_HUNK;
                diff_lines[diff_nlines].is_hunk_header = 1;
            } else if (line[0] == '+' && line[1] != '+') {
                diff_lines[diff_nlines].line_type = LINE_ADD;
            } else if (line[0] == '-' && line[1] != '-') {
                diff_lines[diff_nlines].line_type = LINE_DEL;
            } else if (strncmp(line, "diff ", 5) == 0) {
                diff_lines[diff_nlines].line_type = LINE_HEADER;
            } else if (line[0] == '\\') {
                diff_lines[diff_nlines].line_type = LINE_NORMAL;
            }
            diff_nlines++;
        }
        if (diff_output[offset] == '\n') offset++;
        if (diff_nlines >= MAX_DIFF_LINES - 1) break;
    }

    int current_file_line = 0;
    for (int i = 0; i < diff_nlines; i++) {
        char *line = diff_output + diff_lines[i].offset;

        if (diff_lines[i].line_type == LINE_HUNK) {
            int a = 0, b = 0;
            if (sscanf(line, "@@ -%d,%d +%*d,%*d @@", &a, &b) == 2 ||
                sscanf(line, "@@ -%d +%d,%*d @@", &a, &b) == 2 ||
                sscanf(line, "@@ -%d,%d +%*d @@", &a, &b) == 2 ||
                sscanf(line, "@@ -%d +%d @@", &a, &b) == 2) {
                current_file_line = a - 1;
            }
            diff_lines[i].file_line = current_file_line;
        } else if (diff_lines[i].line_type == LINE_ADD) {
            current_file_line++;
            diff_lines[i].file_line = current_file_line;
        } else if (diff_lines[i].line_type == LINE_DEL) {
            diff_lines[i].file_line = current_file_line;
        } else {
            current_file_line++;
            diff_lines[i].file_line = current_file_line;
        }
    }
}

static void goto_diff_line(void)
{
    if (cursor_pos < 0 || cursor_pos >= diff_nlines) return;

    int file_line = diff_lines[cursor_pos].file_line;
    if (file_line <= 0) {
        file_line = 1;
    }

    curbp->b_point = line_to_point(file_line);
    curwp->w_update = TRUE;
}

static int get_git_diff(const char *filename)
{
    char command[1024];
    FILE *fp;
    int offset = 0;

    char *last_slash = strrchr(filename, '/');
    char *dir = NULL;
    char *file = NULL;

    if (last_slash) {
        int dirlen = last_slash - filename;
        dir = malloc(dirlen + 1);
        strncpy(dir, filename, dirlen);
        dir[dirlen] = '\0';
        file = last_slash + 1;
    }

    if (dir && file) {
        snprintf(command, sizeof(command), "cd '%s' && git diff --no-color -- '%s' 2>/dev/null", dir, file);
    } else {
        snprintf(command, sizeof(command), "git diff --no-color '%s' 2>/dev/null", filename);
    }

    if (dir) free(dir);

    fp = popen(command, "r");
    if (!fp) {
        diff_output[0] = '\0';
        return 0;
    }

    while (fgets(diff_output + offset, DIFF_BUF - offset, fp) != NULL) {
        offset += strlen(diff_output + offset);
        if (offset >= DIFF_BUF - 1) break;
    }

    pclose(fp);
    return offset > 0;
}

void show_git_diff(void)
{
    char *fname = curbp->b_fname;

    if (fname[0] == '\0') {
        msg("No file name for this buffer");
        return;
    }

    if (!get_git_diff(fname)) {
        msg("No git diff available (not in git repo or no changes)");
        return;
    }

    parse_diff_lines();

    if (diff_nlines == 0) {
        msg("No diff output");
        return;
    }

    cursor_pos = 0;
    view_start = 0;

    int c;
    int running = 1;

    draw_diff_view();

    while (running) {
        c = getch();

        if (c == 0x1b) {
            timeout(50);
            int c2 = getch();
            timeout(-1);
            if (c2 == ERR) {
                running = 0;
                continue;
            }
            if (c2 == 0x5b) {
                int c3 = getch();
                if (c3 == 0x41) {
                    if (cursor_pos > 0) {
                        cursor_pos--;
                        if (cursor_pos < view_start) view_start = cursor_pos;
                        draw_diff_view();
                    }
                } else if (c3 == 0x42) {
                    if (cursor_pos < diff_nlines - 1) {
                        cursor_pos++;
                        if (cursor_pos >= view_start + LINES - 3) view_start = cursor_pos - LINES + 4;
                        if (view_start < 0) view_start = 0;
                        draw_diff_view();
                    }
                }
            }
            continue;
        }

        if (c == 'q' || c == 'Q' || c == 0x07) {
            running = 0;
            continue;
        }

        if (c == '\n' || c == '\r') {
            goto_diff_line();
            running = 0;
            continue;
        }

        if (c == 0x10) {
            if (cursor_pos > 0) {
                cursor_pos--;
                if (cursor_pos < view_start) view_start = cursor_pos;
                draw_diff_view();
            }
            continue;
        }

        if (c == 0x0e) {
            if (cursor_pos < diff_nlines - 1) {
                cursor_pos++;
                if (cursor_pos >= view_start + LINES - 3) view_start = cursor_pos - LINES + 4;
                if (view_start < 0) view_start = 0;
                draw_diff_view();
            }
            continue;
        }
    }

    redraw();
}
