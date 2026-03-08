/* gotodef.c, Go to definition, Atto Emacs, Public Domain */

#include "header.h"

#define MAX_RESULTS    512
#define MAX_RESULT_LEN 512
#define MAX_WORD_LEN   128

typedef struct {
	char file[NAME_MAX + 1];
	int line_no;
	char text[MAX_RESULT_LEN];
} def_result_t;

static def_result_t results[MAX_RESULTS];
static int nresults;

static int is_ident_char(char_t c)
{
	return isalnum(c) || c == '_';
}

static int get_word_at_point(buffer_t *bp, char *word, int maxlen)
{
	point_t end_p = pos(bp, bp->b_ebuf);
	point_t p = bp->b_point;
	point_t start, end;
	int i;

	if (p >= end_p) return 0;
	if (!is_ident_char(*ptr(bp, p))) return 0;

	start = p;
	while (start > 0 && is_ident_char(*ptr(bp, start - 1)))
		start--;

	end = p;
	while (end < end_p && is_ident_char(*ptr(bp, end)))
		end++;

	if (end - start <= 0 || end - start >= maxlen) return 0;

	for (i = 0, p = start; p < end && i < maxlen - 1; p++, i++)
		word[i] = *ptr(bp, p);
	word[i] = '\0';
	return i;
}

static void search_definitions(const char *word)
{
	char command[4096];
	char line[MAX_RESULT_LEN * 2];
	FILE *fp;
	int len;

	nresults = 0;

	snprintf(command, sizeof(command),
		"grep -rn "
		"--include='*.c' --include='*.h' --include='*.cpp' --include='*.hpp' "
		"--include='*.cc' --include='*.cxx' "
		"--include='*.go' --include='*.py' --include='*.rb' "
		"--include='*.php' --include='*.zig' --include='*.cr' "
		"--include='*.rs' --include='*.v' "
		"--exclude-dir=.git --exclude-dir=node_modules "
		"--exclude-dir=__pycache__ --exclude-dir=target "
		"--exclude-dir=build --exclude-dir=.zig-cache "
		"-E '"
		"\\b(fn|func|def|function)\\s+%s\\b"
		"|\\b(class|struct|enum|type|module|macro|trait|interface)\\s+%s\\b"
		"|\\bimpl(<.*>)?\\s+%s\\b"
		"|#define\\s+%s\\b"
		"|\\b(var|let|const|static)\\s+(mut\\s+)?%s\\b"
		"|^[a-zA-Z_].*\\b%s\\s*\\("
		"' . 2>/dev/null | head -500",
		word, word, word, word, word, word);

	fp = popen(command, "r");
	if (fp == NULL) return;

	while (fgets(line, sizeof(line), fp) != NULL && nresults < MAX_RESULTS) {
		char *p = line;
		char *colon1, *colon2, *content;
		int flen;

		if (p[0] == '.' && p[1] == '/')
			p += 2;

		colon1 = strchr(p, ':');
		if (colon1 == NULL) continue;
		colon2 = strchr(colon1 + 1, ':');
		if (colon2 == NULL) continue;

		flen = colon1 - p;
		if (flen >= NAME_MAX) flen = NAME_MAX;
		strncpy(results[nresults].file, p, flen);
		results[nresults].file[flen] = '\0';

		results[nresults].line_no = atoi(colon1 + 1);
		if (results[nresults].line_no <= 0) continue;

		content = colon2 + 1;
		while (*content == ' ' || *content == '\t') content++;
		strncpy(results[nresults].text, content, MAX_RESULT_LEN - 1);
		results[nresults].text[MAX_RESULT_LEN - 1] = '\0';
		len = strlen(results[nresults].text);
		while (len > 0 && (results[nresults].text[len-1] == '\n' ||
		                    results[nresults].text[len-1] == '\r'))
			results[nresults].text[--len] = '\0';

		nresults++;
	}

	pclose(fp);
}

static void open_file_at_line(const char *fname, int line_no)
{
	point_t p;

	if (strcmp(fname, curbp->b_fname) == 0) {
		p = line_to_point(line_no);
		if (p != -1) {
			curbp->b_point = p;
			center_cursor();
		}
		return;
	}

	buffer_t *bp = find_buffer((char *)fname, TRUE);
	disassociate_b(curwp);
	curbp = bp;
	associate_b2w(curbp, curwp);

	if (bp->b_fname[0] == '\0') {
		if (!load_file((char *)fname))
			msg("New file %s", fname);
		strncpy(curbp->b_fname, fname, NAME_MAX);
		curbp->b_fname[NAME_MAX] = '\0';
	}

	p = line_to_point(line_no);
	if (p != -1) {
		curbp->b_point = p;
		center_cursor();
	}
}

void gotodef()
{
	char word[MAX_WORD_LEN];
	int i;

	if (!get_word_at_point(curbp, word, MAX_WORD_LEN)) {
		msg("No identifier under cursor");
		return;
	}

	msg("Searching for '%s'...", word);
	dispmsg();
	refresh();

	search_definitions(word);

	if (nresults == 0) {
		msg("No definition found for '%s'", word);
		return;
	}

	if (nresults == 1) {
		open_file_at_line(results[0].file, results[0].line_no);
		msg("%s:%d: %s", results[0].file, results[0].line_no, results[0].text);
		return;
	}

	int selected = 0;
	int top_item = 0;
	int max_rows = LINES - 2;
	int c;
	char title[256];
	char display_line[1024];

	for (;;) {
		attron(A_REVERSE);
		move(0, 0);
		snprintf(title, sizeof(title),
			 " '%s' (%d matches) [Enter=jump Esc=cancel]",
			 word, nresults);
		addstr(title);
		for (i = (int)strlen(title); i < COLS; i++)
			addch(' ');
		attroff(A_REVERSE);

		if (selected < top_item)
			top_item = selected;
		if (selected >= top_item + max_rows)
			top_item = selected - max_rows + 1;

		for (i = 0; i < max_rows; i++) {
			int idx = top_item + i;
			move(i + 1, 0);
			clrtoeol();
			if (idx < nresults) {
				int maxw = COLS < (int)sizeof(display_line)
					? COLS : (int)sizeof(display_line) - 1;
				snprintf(display_line, maxw, " %s:%d: %s",
					 results[idx].file,
					 results[idx].line_no,
					 results[idx].text);
				if (idx == selected)
					attron(A_REVERSE);
				addstr(display_line);
				if (idx == selected) {
					int pad;
					for (pad = (int)strlen(display_line); pad < COLS; pad++)
						addch(' ');
					attroff(A_REVERSE);
				}
			}
		}

		move(LINES - 1, 0);
		clrtoeol();
		refresh();

		c = getch();

		if (c == 0x1b) {
			timeout(50);
			int c2 = getch();
			timeout(-1);
			if (c2 == ERR) {
				redraw();
				return;
			}
			if (c2 == 0x5b) {
				int c3 = getch();
				switch (c3) {
				case 0x41: /* up */
					if (selected > 0) selected--;
					break;
				case 0x42: /* down */
					if (selected < nresults - 1) selected++;
					break;
				case 0x48: /* home */
					selected = 0;
					break;
				case 0x46: /* end */
					selected = nresults - 1;
					break;
				case 0x35: /* pgup */
					getch();
					selected -= max_rows;
					if (selected < 0) selected = 0;
					break;
				case 0x36: /* pgdn */
					getch();
					selected += max_rows;
					if (selected >= nresults)
						selected = nresults - 1;
					break;
				}
			} else if (c2 == 0x4f) {
				int c3 = getch();
				if (c3 == 0x48) selected = 0;
				else if (c3 == 0x46) selected = nresults - 1;
			} else if (c2 == 0x76) {
				selected -= max_rows;
				if (selected < 0) selected = 0;
			} else if (c2 == 0x3c) {
				selected = 0;
			} else if (c2 == 0x3e) {
				selected = nresults - 1;
			}
			continue;
		}

		switch (c) {
		case '\n':
		case '\r':
			open_file_at_line(results[selected].file,
					  results[selected].line_no);
			msg("%s:%d: %s", results[selected].file,
			    results[selected].line_no, results[selected].text);
			redraw();
			return;
		case 0x10: /* C-p */
		case 'k':
			if (selected > 0) selected--;
			break;
		case 0x0e: /* C-n */
		case 'j':
			if (selected < nresults - 1) selected++;
			break;
		case 0x16: /* C-v */
			selected += max_rows;
			if (selected >= nresults) selected = nresults - 1;
			break;
		case 0x07: /* C-g */
			redraw();
			return;
		}
	}
}
