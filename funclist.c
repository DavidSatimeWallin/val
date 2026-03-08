/* funclist.c, Function list navigation, Val Emacs, Public Domain */

#include "header.h"

#define MAX_FUNCS    2048
#define MAX_LINE_LEN 512

typedef struct {
	int line_no;
	char text[MAX_LINE_LEN];
} func_entry_t;

typedef enum {
	LANG_UNKNOWN, LANG_C, LANG_GO, LANG_VLANG, LANG_PYTHON,
	LANG_RUBY, LANG_PHP, LANG_ZIG, LANG_CRYSTAL, LANG_RUST
} lang_t;

static func_entry_t funcs[MAX_FUNCS];
static int nfuncs;

static lang_t detect_language(char *fname)
{
	char *ext = strrchr(fname, '.');
	if (ext == NULL) return LANG_UNKNOWN;
	if (strcmp(ext, ".c") == 0 || strcmp(ext, ".h") == 0 ||
	    strcmp(ext, ".cpp") == 0 || strcmp(ext, ".cc") == 0 ||
	    strcmp(ext, ".cxx") == 0 || strcmp(ext, ".hpp") == 0)
		return LANG_C;
	if (strcmp(ext, ".go") == 0)  return LANG_GO;
	if (strcmp(ext, ".v") == 0)   return LANG_VLANG;
	if (strcmp(ext, ".py") == 0)  return LANG_PYTHON;
	if (strcmp(ext, ".rb") == 0)  return LANG_RUBY;
	if (strcmp(ext, ".php") == 0) return LANG_PHP;
	if (strcmp(ext, ".zig") == 0) return LANG_ZIG;
	if (strcmp(ext, ".cr") == 0)  return LANG_CRYSTAL;
	if (strcmp(ext, ".rs") == 0)  return LANG_RUST;
	return LANG_UNKNOWN;
}

static int get_line_text(buffer_t *bp, point_t start, char *buf, int bufsize)
{
	point_t end_p = pos(bp, bp->b_ebuf);
	int i = 0;
	point_t p = start;
	while (p < end_p && i < bufsize - 1) {
		char_t c = *ptr(bp, p);
		if (c == '\n') break;
		buf[i++] = c;
		p++;
	}
	buf[i] = '\0';
	return i;
}

static int has_prefix(const char *s, const char *prefix)
{
	return strncmp(s, prefix, strlen(prefix)) == 0;
}

static const char *skip_ws(const char *s)
{
	while (*s == ' ' || *s == '\t') s++;
	return s;
}

static void trim_tail(char *s)
{
	int len = strlen(s);
	while (len > 0 && (s[len-1] == ' ' || s[len-1] == '\t' ||
	                    s[len-1] == '{' || s[len-1] == ':' || s[len-1] == '\r'))
		s[--len] = '\0';
}

static void add_func(int line_no, const char *text)
{
	if (nfuncs >= MAX_FUNCS) return;
	funcs[nfuncs].line_no = line_no;
	strncpy(funcs[nfuncs].text, text, MAX_LINE_LEN - 1);
	funcs[nfuncs].text[MAX_LINE_LEN - 1] = '\0';
	trim_tail(funcs[nfuncs].text);
	nfuncs++;
}

static int is_c_func(const char *line)
{
	if (line[0] == '\0' || line[0] == '#' || line[0] == ' ' || line[0] == '\t' ||
	    line[0] == '/' || line[0] == '*' || line[0] == '{' || line[0] == '}')
		return 0;
	if (!strchr(line, '(')) return 0;
	if (has_prefix(line, "if ") || has_prefix(line, "if(")) return 0;
	if (has_prefix(line, "for ") || has_prefix(line, "for(")) return 0;
	if (has_prefix(line, "while ") || has_prefix(line, "while(")) return 0;
	if (has_prefix(line, "switch ") || has_prefix(line, "switch(")) return 0;
	if (has_prefix(line, "return ") || has_prefix(line, "return(")) return 0;
	if (has_prefix(line, "typedef ")) return 0;
	return 1;
}

static int is_go_func(const char *line)
{
	return has_prefix(line, "func ");
}

static int is_vlang_func(const char *line)
{
	return has_prefix(line, "fn ") || has_prefix(line, "pub fn ");
}

static int is_python_func(const char *line)
{
	const char *p = skip_ws(line);
	return has_prefix(p, "def ") || has_prefix(p, "async def ") || has_prefix(p, "class ");
}

static int is_ruby_func(const char *line)
{
	const char *p = skip_ws(line);
	return has_prefix(p, "def ") || has_prefix(p, "class ") || has_prefix(p, "module ");
}

static int is_php_func(const char *line)
{
	const char *p = skip_ws(line);
	return has_prefix(p, "function ") ||
	       has_prefix(p, "public function ") || has_prefix(p, "private function ") ||
	       has_prefix(p, "protected function ") || has_prefix(p, "static function ") ||
	       has_prefix(p, "public static function ") || has_prefix(p, "private static function ") ||
	       has_prefix(p, "protected static function ") || has_prefix(p, "class ");
}

static int is_zig_func(const char *line)
{
	const char *p = skip_ws(line);
	return has_prefix(p, "fn ") || has_prefix(p, "pub fn ") || has_prefix(p, "export fn ");
}

static int is_crystal_func(const char *line)
{
	const char *p = skip_ws(line);
	return has_prefix(p, "def ") || has_prefix(p, "class ") ||
	       has_prefix(p, "module ") || has_prefix(p, "macro ");
}

static int is_rust_func(const char *line)
{
	const char *p = skip_ws(line);
	return has_prefix(p, "fn ") || has_prefix(p, "pub fn ") ||
	       has_prefix(p, "pub(crate) fn ") || has_prefix(p, "pub(super) fn ") ||
	       has_prefix(p, "async fn ") || has_prefix(p, "pub async fn ") ||
	       has_prefix(p, "unsafe fn ") || has_prefix(p, "pub unsafe fn ") ||
	       has_prefix(p, "const fn ") || has_prefix(p, "pub const fn ") ||
	       has_prefix(p, "impl ") || has_prefix(p, "impl<");
}

static void scan_buffer(buffer_t *bp, lang_t lang)
{
	char line[MAX_LINE_LEN];
	point_t end_p = pos(bp, bp->b_ebuf);
	point_t p = 0;
	int line_no = 1;

	nfuncs = 0;

	while (p < end_p && nfuncs < MAX_FUNCS) {
		get_line_text(bp, p, line, MAX_LINE_LEN);

		int match = 0;
		switch (lang) {
		case LANG_C:       match = is_c_func(line); break;
		case LANG_GO:      match = is_go_func(line); break;
		case LANG_VLANG:   match = is_vlang_func(line); break;
		case LANG_PYTHON:  match = is_python_func(line); break;
		case LANG_RUBY:    match = is_ruby_func(line); break;
		case LANG_PHP:     match = is_php_func(line); break;
		case LANG_ZIG:     match = is_zig_func(line); break;
		case LANG_CRYSTAL: match = is_crystal_func(line); break;
		case LANG_RUST:    match = is_rust_func(line); break;
		default: break;
		}

		if (match)
			add_func(line_no, line);

		while (p < end_p && *ptr(bp, p) != '\n')
			p++;
		if (p < end_p) p++;
		line_no++;
	}
}

static int strcasestr_match(const char *haystack, const char *needle)
{
	int hlen = strlen(haystack);
	int nlen = strlen(needle);
	for (int i = 0; i <= hlen - nlen; i++) {
		int j;
		for (j = 0; j < nlen; j++)
			if (tolower((unsigned char)haystack[i+j]) != tolower((unsigned char)needle[j]))
				break;
		if (j == nlen) return 1;
	}
	return 0;
}

static int matched[MAX_FUNCS];
static int nmatched;

static void refilter(const char *filter)
{
	nmatched = 0;
	for (int i = 0; i < nfuncs; i++) {
		if (filter[0] == '\0' || strcasestr_match(funcs[i].text, filter))
			matched[nmatched++] = i;
	}
}

void funclist()
{
	lang_t lang = detect_language(curbp->b_fname);
	if (lang == LANG_UNKNOWN) {
		msg("Function list: unsupported file type");
		return;
	}

	scan_buffer(curbp, lang);

	if (nfuncs == 0) {
		msg("No functions found");
		return;
	}

	int selected = 0;
	int top_item = 0;
	int max_rows = LINES - 3;
	int c, i;
	char title[256];
	char display_line[1024];
	char filter[STRBUF_M];
	int flen = 0;
	filter[0] = '\0';

	refilter(filter);

	for (;;) {
		/* title bar */
		attron(A_REVERSE);
		move(0, 0);
		snprintf(title, sizeof(title),
			 " Functions (%d/%d) [Enter=jump Esc=cancel]", nmatched, nfuncs);
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
			if (idx < nmatched) {
				int fi = matched[idx];
				int maxw = COLS < (int)sizeof(display_line) ? COLS : (int)sizeof(display_line) - 1;
				snprintf(display_line, maxw, " %4d: %s",
					 funcs[fi].line_no, funcs[fi].text);
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

		/* search bar */
		move(LINES - 2, 0);
		attron(A_REVERSE);
		snprintf(display_line, COLS, " Search: %s", filter);
		addstr(display_line);
		for (i = (int)strlen(display_line); i < COLS; i++)
			addch(' ');
		attroff(A_REVERSE);

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
					if (selected < nmatched - 1) selected++;
					break;
				case 0x48: /* home */
					selected = 0;
					break;
				case 0x46: /* end */
					selected = nmatched - 1;
					break;
				case 0x35: /* pgup: ESC [ 5 ~ */
					getch();
					selected -= max_rows;
					if (selected < 0) selected = 0;
					break;
				case 0x36: /* pgdn: ESC [ 6 ~ */
					getch();
					selected += max_rows;
					if (selected >= nmatched) selected = nmatched - 1;
					break;
				}
			} else if (c2 == 0x4f) {
				int c3 = getch();
				if (c3 == 0x48) selected = 0;
				else if (c3 == 0x46) selected = nmatched - 1;
			} else if (c2 == 0x76) {
				selected -= max_rows;
				if (selected < 0) selected = 0;
			} else if (c2 == 0x3c) {
				selected = 0;
			} else if (c2 == 0x3e) {
				selected = nmatched - 1;
			}
			continue;
		}

		switch (c) {
		case '\n':
		case '\r':
			if (nmatched > 0) {
				int fi = matched[selected];
				point_t p = line_to_point(funcs[fi].line_no);
				if (p != -1) {
					curbp->b_point = p;
					center_cursor();
					msg("Line %d: %s", funcs[fi].line_no, funcs[fi].text);
				}
			}
			redraw();
			return;
		case 0x10: /* C-p */
			if (selected > 0) selected--;
			break;
		case 0x0e: /* C-n */
			if (selected < nmatched - 1) selected++;
			break;
		case 0x16: /* C-v — page down */
			selected += max_rows;
			if (selected >= nmatched) selected = nmatched - 1;
			break;
		case 0x07: /* C-g — cancel */
			redraw();
			return;
		case 0x7f: /* del */
		case 0x08: /* backspace */
			if (flen > 0) {
				filter[--flen] = '\0';
				refilter(filter);
				selected = 0;
				top_item = 0;
			}
			break;
		default:
			if (c >= 32 && c < 127 && flen < STRBUF_M - 2) {
				filter[flen++] = c;
				filter[flen] = '\0';
				refilter(filter);
				selected = 0;
				top_item = 0;
			}
			break;
		}
	}
}
