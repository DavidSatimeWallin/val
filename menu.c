/* menu.c, Val Emacs menu bar, Public Domain, 2026 */

#include "header.h"

typedef struct {
	const char *shortcut;
	const char *label;
	void (*func)(void);
} menu_item_t;

typedef struct {
	const char *label;
	int nitems;
	const menu_item_t *items;
} menu_t;

static const menu_item_t file_items[] = {
	{"C-x C-f", "Find File",       readfile  },
	{"C-x C-s", "Save Buffer",     savebuffer},
	{"C-x C-w", "Write File As",   writefile },
	{"C-x i",   "Insert File",     insertfile},
	{"C-x k",   "Kill Buffer",     killbuffer},
	{"C-x C-c", "Exit",            quit_ask  },
};

static const menu_item_t edit_items[] = {
	{"C-_",   "Undo",           undo      },
	{"M-_",   "Redo",           redo      },
	{"C-w",   "Cut Region",     cut       },
	{"M-w",   "Copy Region",    copy      },
	{"C-y",   "Paste",          paste     },
	{"C-d",   "Delete",         delete    },
	{"C-k",   "Kill to EOL",    killtoeol },
	{"C-SPC", "Set Mark",       iblock    },
	{"C-x r i", "Indent Tabs",  indent_to_tabs},
};

static const menu_item_t nav_items[] = {
	{"C-n",   "Next Line",       down    },
	{"C-p",   "Prev Line",       up      },
	{"C-f",   "Forward Char",    right   },
	{"C-b",   "Backward Char",   left    },
	{"C-a",   "Beginning Line",  lnbegin },
	{"C-e",   "End Line",        lnend   },
	{"C-v",   "Forward Page",    pgdown  },
	{"M-v",   "Backward Page",   pgup    },
	{"M-<",   "Beginning Buf",   top     },
	{"M->",   "End Buffer",      bottom  },
	{"M-f",   "Forward Word",    wright  },
	{"M-b",   "Backward Word",   wleft   },
	{"M-g",   "Go to Line",      gotoline},
	{"M-j",   "Jump Line",       jump    },
};

static const menu_item_t search_items[] = {
	{"C-s", "Search",          search        },
	{"C-r", "Search Back",     search        },
	{"M-r", "Query Replace",   query_replace },
};

static const menu_item_t buffer_items[] = {
	{"C-x C-n", "Next Buffer",     next_buffer         },
	{"C-x o",   "Other Window",    next_window         },
	{"C-x 2",   "Split Window",    split_window        },
	{"C-x 1",   "Delete Others",   delete_other_windows},
	{"Ins",     "Overwrite Mode",  toggle_overwrite_mode},
};

static const menu_item_t tools_items[] = {
	{"M-l",   "Function List",   funclist         },
	{"M-,",   "Go to Def",       gotodef          },
	{"M-.",   "FZF Find File",   fzf_find_file    },
	{"M--",   "Shell Command",   shell_cmd        },
	{"M-d",   "Git Diff",        show_git_diff    },
	{"C-x h", "Keyboard Help",   show_keyboard_help},
	{"C-l",   "Refresh",         redraw           },
	{"M-ESC", "Version",         version          },
};

#define N_GROUPS 6
static const menu_t menu_groups[N_GROUPS] = {
	{"File",     6, file_items   },
	{"Edit",     9, edit_items   },
	{"Navigate", 14, nav_items    },
	{"Search",   3, search_items },
	{"Buffer",   5, buffer_items },
	{"Tools",    8, tools_items  },
};

static int menu_xpos[N_GROUPS];
static int menu_active = 0;
static int menu_group = 0;
static int menu_item = 0;
static int show_dropdown = 0;

static void calc_menu_positions(void)
{
	int x = 1;
	for (int i = 0; i < N_GROUPS; i++) {
		menu_xpos[i] = x;
		x += strlen(menu_groups[i].label) + 3;
	}
}

void draw_menu_bar(void)
{
	if (LINES < 5) return;
	calc_menu_positions();

	move(0, 0);
	clrtoeol();

	for (int i = 0; i < N_GROUPS; i++) {
		int x = menu_xpos[i];
		if (menu_active && i == menu_group) {
			attron(A_STANDOUT);
			mvaddstr(0, x, menu_groups[i].label);
			attroff(A_STANDOUT);
		} else {
			mvaddstr(0, x, menu_groups[i].label);
		}
	}

	move(1, 0);
	attron(A_DIM);
	for (int x = 0; x < COLS; x++)
		addch('-');
	attroff(A_DIM);
}

static void draw_dropdown(void)
{
	const menu_t *grp = &menu_groups[menu_group];
	int n = grp->nitems;

	/* find max label width */
	int max_label = 0, max_short = 0;
	for (int i = 0; i < n; i++) {
		int l = strlen(grp->items[i].label);
		int s = strlen(grp->items[i].shortcut);
		if (l > max_label) max_label = l;
		if (s > max_short) max_short = s;
	}

	int box_w = max_label + max_short + 6;
	int box_x = menu_xpos[menu_group];
	int box_y = 2;

	if (box_x + box_w >= COLS)
		box_x = COLS - box_w - 1;
	if (box_x < 0) box_x = 0;

	int max_items = LINES - box_y - 2;
	if (max_items < 1) return;
	if (n > max_items) n = max_items;

	/* draw top border */
	attrset(A_NORMAL);
	move(box_y, box_x);
	addch('+');
	for (int x = 1; x < box_w - 1; x++) addch('-');
	addch('+');

	/* draw items */
	for (int i = 0; i < n; i++) {
		move(box_y + 1 + i, box_x);
		addch('|');
		addch(' ');
		if (menu_active && show_dropdown && i == menu_item) {
			attron(A_REVERSE);
		}
		addstr(grp->items[i].label);
		int pad = max_label - strlen(grp->items[i].label);
		for (int p = 0; p < pad; p++) addch(' ');
		addstr("  ");
		addstr(grp->items[i].shortcut);
		if (menu_active && show_dropdown && i == menu_item) {
			attroff(A_REVERSE);
		}
		pad = max_short - strlen(grp->items[i].shortcut);
		for (int p = 0; p < pad; p++) addch(' ');
		addch(' ');
		addch('|');
	}

	/* draw bottom border */
	move(box_y + 1 + n, box_x);
	addch('+');
	for (int x = 1; x < box_w - 1; x++) addch('-');
	addch('+');

	attrset(A_NORMAL);
}

void menu_activate(void)
{
	if (LINES < 5) return;
	menu_active = 1;
	menu_group = 0;
	menu_item = 0;
	show_dropdown = 0;

	keypad(stdscr, TRUE);
	curs_set(0);

	int ch;
	while (menu_active) {
		update_display();
		draw_menu_bar();
		if (show_dropdown)
			draw_dropdown();
		refresh();

		ch = getch();

		if (ch == KEY_RESIZE || ch == 0x9A) {
			resize_terminal();
			continue;
		}

		if (show_dropdown) {
			switch (ch) {
			case KEY_UP:
				menu_item = (menu_item > 0) ? menu_item - 1 : menu_groups[menu_group].nitems - 1;
				break;
			case KEY_DOWN:
				menu_item = (menu_item + 1) % menu_groups[menu_group].nitems;
				break;
			case KEY_LEFT:
				show_dropdown = 0;
				menu_group = (menu_group > 0) ? menu_group - 1 : N_GROUPS - 1;
				menu_item = 0;
				break;
			case KEY_RIGHT:
				show_dropdown = 0;
				menu_group = (menu_group + 1) % N_GROUPS;
				menu_item = 0;
				break;
			case '\n':
			case '\r':
			case ' ':
				if (menu_groups[menu_group].items[menu_item].func) {
					void (*f)(void) = menu_groups[menu_group].items[menu_item].func;
					menu_active = 0;
					show_dropdown = 0;
					curs_set(1);
					keypad(stdscr, FALSE);
					redraw();
					f();
					return;
				}
				break;
			case 27:
				show_dropdown = 0;
				break;
			}
		} else {
			switch (ch) {
			case KEY_LEFT:
				menu_group = (menu_group > 0) ? menu_group - 1 : N_GROUPS - 1;
				menu_item = 0;
				break;
			case KEY_RIGHT:
				menu_group = (menu_group + 1) % N_GROUPS;
				menu_item = 0;
				break;
			case KEY_DOWN:
			case '\n':
			case '\r':
				show_dropdown = 1;
				menu_item = 0;
				break;
			case KEY_UP:
				menu_group = (menu_group > 0) ? menu_group - 1 : N_GROUPS - 1;
				menu_item = 0;
				break;
			case 27:
				menu_active = 0;
				break;
			}
		}
	}

	curs_set(1);
	keypad(stdscr, FALSE);
	redraw();
}
