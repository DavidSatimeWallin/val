/* command.c, Val Emacs, Public Domain, Hugh Barney, 2016, Derived from: Anthony's Editor January 93 */

#include "header.h"

void quit() { done = 1; }
void up() { curbp->b_point = lncolumn(curbp, upup(curbp, curbp->b_point),curbp->b_col); }
void down() { curbp->b_point = lncolumn(curbp, dndn(curbp, curbp->b_point),curbp->b_col); }
void lnbegin() { curbp->b_point = segstart(curbp, lnstart(curbp,curbp->b_point), curbp->b_point); }
void version() { msg(VERSION); }
void top() { curbp->b_point = 0; }
void bottom() { curbp->b_point = pos(curbp, curbp->b_ebuf); if (curbp->b_epage < pos(curbp, curbp->b_ebuf)) curbp->b_reframe = 1;}
void block() { curbp->b_mark = curbp->b_point; }
void copy() { copy_cut(FALSE); }
void cut() { copy_cut(TRUE); }
void resize_terminal() { one_window(curwp); }

void indent_to_tabs(void)
{
	point_t orig_point = curbp->b_point;
	int changes = 0;
	
	point_t end_pos = pos(curbp, curbp->b_ebuf);
	int bufsize = end_pos;
	
	char *tmp = malloc(bufsize + 1);
	if (!tmp) {
		msg("Failed to allocate memory");
		return;
	}
	
	curbp->b_point = movegap(curbp, 0);
	for (int i = 0; i < bufsize; i++) {
		tmp[i] = curbp->b_egap[i];
	}
	
	char *out = malloc(bufsize * 8 + 1);
	if (!out) {
		free(tmp);
		msg("Failed to allocate memory");
		return;
	}
	
	int out_len = 0;
	int i = 0;
	
	while (i < bufsize) {
		if (tmp[i] == '\n') {
			out[out_len++] = '\n';
			i++;
		} else if (tmp[i] == ' ' && (out_len == 0 || out[out_len - 1] == '\n')) {
			int spaces = 0;
			while (i + spaces < bufsize && tmp[i + spaces] == ' ' && spaces < 8) {
				spaces++;
			}
			
			if (i + spaces < bufsize && tmp[i + spaces] == '\n') {
				i += spaces;
			} else {
				int tabs = spaces / 8;
				int leftover = spaces % 8;
				
				for (int j = 0; j < tabs; j++) {
					out[out_len++] = '\t';
				}
				for (int j = 0; j < leftover; j++) {
					out[out_len++] = ' ';
				}
				
				if (tabs > 0 || leftover > 0) {
					changes++;
				}
				i += spaces;
			}
		} else {
			out[out_len++] = tmp[i++];
		}
	}
	
	free(tmp);
	
	curbp->b_point = 0;
	curbp->b_gap = curbp->b_buf;
	curbp->b_egap = curbp->b_buf;
	curbp->b_ebuf = curbp->b_buf;
	
	if (!growgap(curbp, out_len + 1024)) {
		free(out);
		msg("Failed to grow buffer");
		return;
	}
	
	curbp->b_point = movegap(curbp, 0);
	memcpy(curbp->b_gap, out, out_len);
	curbp->b_gap += out_len;
	curbp->b_point = pos(curbp, curbp->b_egap);
	
	free(out);
	
	if (orig_point > out_len) {
		orig_point = out_len;
	}
	curbp->b_point = orig_point;
	curbp->b_flags |= B_MODIFIED;
	
	msg("Replaced indentation with tabs (%d lines changed)", changes);
}

void quit_ask()
{
	if (modified_buffers() > 0) {
		mvaddstr(MSGLINE, 0, "Modified buffers exist; really exit (y/n) ?");
		clrtoeol();
		if (!yesno(FALSE))
			return;
	}
	quit();
}

/* flag = default answer, FALSE=n, TRUE=y */
int yesno(int flag)
{
	int ch;

	addstr(flag ? " y\b" : " n\b");
	refresh();
	ch = getch();
	if (ch == '\r' || ch == '\n')
		return (flag);
	return (tolower(ch) == 'y');
}

void redraw()
{
	window_t *wp;
	
	clear();
	for (wp=wheadp; wp != NULL; wp = wp->w_next)
		wp->w_update = TRUE;
	update_display();
}

void left()
{
	int n = prev_utf8_char_size();
	while (0 < curbp->b_point && n-- > 0)
		--curbp->b_point;
}

void right()
{
	int n = utf8_size(*ptr(curbp,curbp->b_point));
	while ((curbp->b_point < pos(curbp, curbp->b_ebuf)) && n-- > 0)
		++curbp->b_point;
}

/* work out number of bytes based on first byte */
int utf8_size(char_t c)
{
	if (c >= 192 && c < 224) return 2;
	if (c >= 224 && c < 240) return 3;
	if (c >= 240 && c < 248) return 4;
	return 1; /* if in doubt it is 1 */
}

int prev_utf8_char_size()
{
	int n;
	for (n=2;n<5;n++)
		if (-1 < curbp->b_point - n && (utf8_size(*(ptr(curbp, curbp->b_point - n))) == n))
			return n;
	return 1;
}

void lnend()
{
        if (curbp->b_point == pos(curbp, curbp->b_ebuf)) return; /* do nothing if EOF */
	curbp->b_point = dndn(curbp, curbp->b_point);
	point_t p = curbp->b_point;
	left();
	curbp->b_point = (*ptr(curbp, curbp->b_point) == '\n') ? curbp->b_point : p;
}

void wleft()
{
	char_t *p;
	while (!isspace(*(p = ptr(curbp, curbp->b_point))) && curbp->b_buf < p)
		--curbp->b_point;
	while (isspace(*(p = ptr(curbp, curbp->b_point))) && curbp->b_buf < p)
		--curbp->b_point;
}

void pgdown()
{
	curbp->b_page = curbp->b_point = upup(curbp, curbp->b_epage);
	while (0 < curbp->b_row--)
		down();
	curbp->b_epage = pos(curbp, curbp->b_ebuf);
}

void pgup()
{
	int i = curwp->w_rows;
	while (0 < --i) {
		curbp->b_page = upup(curbp, curbp->b_page);
		up();
	}
}

void wright()
{
	char_t *p;
	while (!isspace(*(p = ptr(curbp, curbp->b_point))) && p < curbp->b_ebuf)
		++curbp->b_point;
	while (isspace(*(p = ptr(curbp, curbp->b_point))) && p < curbp->b_ebuf)
		++curbp->b_point;
}

static int count_leading_tabs(void)
{
	point_t line_start = lnstart(curbp, curbp->b_point);
	int tabs = 0;
	char_t *p = ptr(curbp, line_start);
	
	while (p < ptr(curbp, curbp->b_point) || (p == ptr(curbp, curbp->b_point) && p < curbp->b_egap)) {
		if (*p == '\t') {
			tabs++;
			p++;
		} else if (*p == ' ') {
			tabs++;
			p++;
		} else {
			break;
		}
	}
	return tabs;
}

static int count_brace_indent(void)
{
	int depth = 0;
	point_t p = 0;
	point_t end = curbp->b_point;
	char_t *cp;
	
	while (p < end) {
		cp = ptr(curbp, p);
		if (*cp == '{') {
			depth++;
		} else if (*cp == '}') {
			depth--;
		}
		p++;
	}
	
	if (depth < 0) depth = 0;
	return depth;
}

static void insert_char(char c)
{
	assert(curbp->b_gap <= curbp->b_egap);
	if (curbp->b_gap == curbp->b_egap && !growgap(curbp, CHUNK))
		return;
	curbp->b_point = movegap(curbp, curbp->b_point);

	if ((curbp->b_flags & B_OVERWRITE) && *(ptr(curbp, curbp->b_point)) != '\n' && curbp->b_point < pos(curbp,curbp->b_ebuf) ) {
		*(ptr(curbp, curbp->b_point)) = c;
		if (curbp->b_point < pos(curbp, curbp->b_ebuf))
			++curbp->b_point;
	} else {
		*curbp->b_gap++ = c;
		curbp->b_point = pos(curbp, curbp->b_egap);
		if (curbp->b_point == pos(curbp, curbp->b_ebuf) && curbp->b_point >= curbp->b_epage) curbp->b_reframe = 1;
	}
	curbp->b_flags |= B_MODIFIED;
}

void insert()
{
	if (*input == '\r' || *input == '\n') {
		int tabs = count_leading_tabs();
		insert_char('\n');
		for (int i = 0; i < tabs; i++) {
			insert_char('\t');
		}
		return;
	}
	
	if (*input == '{') {
		insert_char('{');
		int tabs = count_leading_tabs();
		insert_char('\n');
		for (int i = 0; i < tabs + 1; i++) {
			insert_char('\t');
		}
		return;
	}
	
	if (*input == '}') {
		int tabs = count_brace_indent();
		if (tabs > 0) tabs--;
		insert_char('\n');
		for (int i = 0; i < tabs; i++) {
			insert_char('\t');
		}
		insert_char('}');
		return;
	}
	
	insert_char(*input);
}

void backsp()
{
	curbp->b_point = movegap(curbp, curbp->b_point);
	if (curbp->b_buf < curbp->b_gap) {
		curbp->b_gap -= prev_utf8_char_size();
		curbp->b_flags |= B_MODIFIED;
	}
	curbp->b_point = pos(curbp, curbp->b_egap);
}

void delete()
{
	curbp->b_point = movegap(curbp, curbp->b_point);
	if (curbp->b_egap < curbp->b_ebuf) {
		curbp->b_egap += utf8_size(*curbp->b_egap);
		curbp->b_point = pos(curbp, curbp->b_egap);
		curbp->b_flags |= B_MODIFIED;
	}
}

void jump()
{
	int n, curline, lastline;
	char dir = 0;
	point_t p;

	if (!getinput("Jump (20d, 15u, 18): ", temp, STRBUF_S, F_CLEAR))
		return;

	sscanf(temp, "%d%c", &n, &dir);
	if (dir == 'd' || dir == 'u') {
		get_line_stats(&curline, &lastline);
		n = dir == 'd' ? curline + n : curline - n;
	}

	p = line_to_point(n);
	if (p != -1) {
		curbp->b_point = p;
		msg("Line %d", n);
	} else {
		msg("Line %d not found", n);
	}
}

void gotoline()
{
	int line;
	point_t p;

	if (getinput("Goto line: ", temp, STRBUF_S, F_CLEAR)) {
		line = atoi(temp);
		p = line_to_point(line);
		if (p != -1) {
			curbp->b_point = p;
			msg("Line %d", line);
		} else {
			msg("Line %d, not found", line);
		}
	}
}

void insertfile()
{
	if (getfilename("Insert file: ", temp, NAME_MAX))
		(void)insert_file(temp, TRUE);
}

void readfile()
{
	buffer_t *bp;

	if (getfilename("Find file: ", temp, NAME_MAX)) {
		bp = find_buffer(temp, TRUE);
		disassociate_b(curwp); /* we are leaving the old buffer for a new one */
		curbp = bp;
		associate_b2w(curbp, curwp);

		/* load the file if not already loaded */
		if (bp != NULL && bp->b_fname[0] == '\0') {
			if (!load_file(temp)) {
				msg("New file %s", temp);
			}
			strncpy(curbp->b_fname, temp, NAME_MAX);
			curbp->b_fname[NAME_MAX] = '\0'; /* truncate if required */
		}
	}
}

void savebuffer()
{
	if (curbp->b_fname[0] != '\0') {
		save(curbp->b_fname);
		return;
	} else {
		writefile();
	}
	refresh();
}

void writefile()
{
	strncpy(temp, curbp->b_fname, NAME_MAX);
	if (getinput("Write file: ", temp, NAME_MAX, F_NONE))
		if (save(temp) == TRUE)
			strncpy(curbp->b_fname, temp, NAME_MAX);
}

void killbuffer()
{
	buffer_t *kill_bp = curbp;
	buffer_t *bp;
	int bcount = count_buffers();

	/* do nothing if only buffer left is the scratch buffer */
	if (bcount == 1 && 0 == strcmp(get_buffer_name(curbp), "*scratch*"))
		return;
	
	if (curbp->b_flags & B_MODIFIED) {
		mvaddstr(MSGLINE, 0, "Discard changes (y/n) ?");
		clrtoeol();
		if (!yesno(FALSE))
			return;
	}

	if (bcount == 1) {
		/* create a scratch buffer */
		bp = find_buffer("*scratch*", TRUE);
		strcpy(bp->b_bname, "*scratch*");
	}

	next_buffer();
	assert(kill_bp != curbp);
	delete_buffer(kill_bp);
}

void iblock()
{
	block();
	msg("Mark set");
}

void toggle_overwrite_mode() { curbp->b_flags ^= B_OVERWRITE; }

void killtoeol()
{
        if (curbp->b_point == pos(curbp, curbp->b_ebuf))
		return; /* do nothing if at end of file */
	if (*(ptr(curbp, curbp->b_point)) == 0xa) {
		delete(); /* delete CR if at start of empty line */
	} else {
		curbp->b_mark = curbp->b_point;
		lnend();
		if (curbp->b_mark != curbp->b_point) copy_cut(TRUE);
	}
}

void copy_cut(int cut)
{
	char_t *p;
	/* if no mark or point == marker, nothing doing */
	if (curbp->b_mark == NOMARK || curbp->b_point == curbp->b_mark)
		return;
	if (scrap != NULL) {
		free(scrap);
		scrap = NULL;
	}
	if (curbp->b_point < curbp->b_mark) {
		/* point above marker: move gap under point, region = marker - point */
		(void) movegap(curbp, curbp->b_point);
		p = ptr(curbp, curbp->b_point);
		nscrap = curbp->b_mark - curbp->b_point;
	} else {
		/* if point below marker: move gap under marker, region = point - marker */
		(void) movegap(curbp, curbp->b_mark);
		p = ptr(curbp, curbp->b_mark);
		nscrap = curbp->b_point - curbp->b_mark;
	}
	if ((scrap = (char_t*) malloc(nscrap)) == NULL) {
		msg("No more memory available.");
	} else {
		(void) memcpy(scrap, p, nscrap * sizeof (char_t));
		if (cut) {
			curbp->b_egap += nscrap; /* if cut expand gap down */
			curbp->b_point = pos(curbp, curbp->b_egap); /* set point to after region */
			curbp->b_flags |= B_MODIFIED;
			msg("%ld bytes cut.", nscrap);
		} else {
			msg("%ld bytes copied.", nscrap);
		}
		curbp->b_mark = NOMARK;  /* unmark */
	}
}

void paste()
{
	if(curbp->b_flags & B_OVERWRITE)
		return;
	if (nscrap <= 0) {
		msg("Scrap is empty.  Nothing to paste.");
	} else if (nscrap < curbp->b_egap - curbp->b_gap || growgap(curbp, nscrap)) {
		curbp->b_point = movegap(curbp, curbp->b_point);
		memcpy(curbp->b_gap, scrap, nscrap * sizeof (char_t));
		curbp->b_gap += nscrap;
		curbp->b_point = pos(curbp, curbp->b_egap);
		curbp->b_flags |= B_MODIFIED;
	}
}

void fzf_find_file()
{
	char tempfile[] = "/tmp/val_fzf_XXXXXX";
	char command[NAME_MAX + 64];
	char fname[NAME_MAX + 1];
	FILE *fp;
	int fd;

	if ((fd = mkstemp(tempfile)) < 0) { msg("fzf: failed to create temp file"); return; }
	close(fd);

	snprintf(command, sizeof(command), "/usr/bin/fzf > %s", tempfile);
	def_prog_mode();
	endwin();
	int ret = system(command);
	reset_prog_mode();
	refresh();

	if (ret != 0) { unlink(tempfile); msg("fzf: cancelled"); return; }

	fp = fopen(tempfile, "r");
	unlink(tempfile);
	if (fp == NULL) { msg("fzf: failed to read result"); return; }
	if (fgets(fname, NAME_MAX, fp) == NULL) { fclose(fp); msg("fzf: no file selected"); return; }
	fclose(fp);

	fname[strcspn(fname, "\n")] = '\0';

	disassociate_b(curwp);
	curbp = find_buffer(fname, TRUE);
	associate_b2w(curbp, curwp);

	if (curbp->b_fname[0] == '\0') {
		if (!load_file(fname))
			msg("New file %s", fname);
		strncpy(curbp->b_fname, fname, NAME_MAX);
		curbp->b_fname[NAME_MAX] = '\0';
	}
}

void shell_cmd()
{
	int c;

	temp[0] = '\0';
	if (!getinput("Shell command: ", temp, TEMPBUF, F_CLEAR))
		return;

	def_prog_mode();
	endwin();
	printf("\n$ %s\n\n", temp);
	(void)!system(temp);
	printf("\n[Press ENTER to return to editor]");
	fflush(stdout);
	while ((c = getchar()) != '\n' && c != EOF)
		;
	reset_prog_mode();
	refresh();
}

void showpos()
{
	int current, lastln;
	point_t end_p = pos(curbp, curbp->b_ebuf);
    
	get_line_stats(&current, &lastln);

	if (curbp->b_point == end_p) {
		msg("[EOB] Line = %d/%d  Point = %d/%d", current, lastln,
			curbp->b_point, ((curbp->b_ebuf - curbp->b_buf) - (curbp->b_egap - curbp->b_gap)));
	} else {
		msg("Char = %s 0x%x  Line = %d/%d  Point = %d/%d", unctrl(*(ptr(curbp, curbp->b_point))), *(ptr(curbp, curbp->b_point)), 
			current, lastln,
			curbp->b_point, ((curbp->b_ebuf - curbp->b_buf) - (curbp->b_egap - curbp->b_gap)));
	}
}
