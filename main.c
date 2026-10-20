/*
 * main.c - process startup, global editor state, and the command loop.
 *
 * Val is deliberately single-threaded.  Each loop iteration paints the current
 * state, decodes one key or UTF-8 character, and invokes exactly one command.
 * Commands communicate short status messages through msg() and mutate the
 * current buffer/window through the globals declared in header.h.
 *
 * Public Domain, David Satime Wallin 2026, Hugh Barney 2016.
 * Derived from Anthony's Editor, January 1993.
 */

#include "header.h"

/* Process-wide state.  Keeping it here makes ownership and cleanup explicit. */
int done;                       /* Set by quit_ask() to leave the main loop. */
point_t nscrap;                 /* Number of bytes in the cut/copy buffer. */
char_t *scrap;                  /* Heap-owned cut/copy buffer, or NULL. */

char_t *input;                  /* Bytes returned by get_key(); not owned here. */
int msgflag;
char msgline[TEMPBUF];
char temp[TEMPBUF];
char searchtext[STRBUF_M];
char replace[STRBUF_M];

keymap_t *key_return;
keymap_t *key_map;
buffer_t *curbp;			/* current buffer */
buffer_t *bheadp;			/* head of list of buffers */
window_t *curwp;
window_t *wheadp;

int main(int argc, char **argv)
{
	setlocale(LC_ALL, "") ; /* required for 3,4 byte UTF8 chars */
	if (initscr() == NULL) fatal("%s: Failed to initialize the screen.\n");
	raw();
	noecho();
	idlok(stdscr, TRUE);

	start_color();
	use_default_colors();
	init_pair(ID_MODELINE, COLOR_BLACK, COLOR_WHITE);
	init_pair(ID_COMMENT, COLOR_BLUE, -1);
	init_pair(ID_CURSOR_LINE, COLOR_BLACK, COLOR_CYAN);
	
	if (argc > 1) {
		curbp = find_buffer(argv[1], TRUE);
		(void) insert_file(argv[1], FALSE);
		/* Save filename regardless of load() success. */
		strncpy(curbp->b_fname, argv[1], NAME_MAX);
		curbp->b_fname[NAME_MAX] = '\0'; /* force truncation */
	} else {
		curbp = find_buffer("*scratch*", TRUE);
		strncpy(curbp->b_bname, "*scratch*", STRBUF_S);
	}

	wheadp = curwp = new_window();
	one_window(curwp);
	associate_b2w(curbp, curwp);

	if (!growgap(curbp, CHUNK)) fatal("%s: Failed to allocate required memory.\n");
	key_map = keymap;

	/*
	 * Commands do not repaint directly in the normal editing path.  Repainting
	 * at the top of the loop gives every command a consistent completed frame.
	 */
	while (!done) {
		update_display();
		input = get_key(key_map, &key_return);

		if (key_return != NULL) {
			(key_return->func)();
		} else {
			/* allow TAB and NEWLINE, otherwise any Control Char is 'Not bound' */
			if (*input > 31 || *input == 10 || *input == 9)
				insert();
                        else {
				flushinp(); /* discard without writing in buffer */
				msg("Not bound");
			}
		}
	}

	if (scrap != NULL) free(scrap);
	move(LINES-1, 0);
	refresh();
	noraw();
	endwin();
	return 0;
}

_Noreturn void fatal(char *fmt)
{
	if (curscr != NULL) {
		move(LINES-1, 0);
		refresh();
		noraw();
		endwin();
		putchar('\n');
	}
	fprintf(stderr, fmt, PROG_NAME);
	exit(1);
}

void msg(char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	(void)vsnprintf(msgline, TEMPBUF, fmt, args);
	va_end(args);
	msgflag = TRUE;
}
