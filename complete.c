/*
 * complete.c - filename minibuffer with shell-assisted tab completion.
 *
 * Completion writes shell expansion results to a private temporary file, then
 * cycles through space-separated candidates on repeated Tab presses.  The
 * caller supplies the destination capacity; every direct append honors it.
 *
 * Public Domain, David Satime Wallin 2026, Hugh Barney 2016.
 */

#include "header.h"

/* basic filename completion, based on code in uemacs/PK */
/* Return TRUE for a non-empty accepted filename and FALSE when cancelled. */
int getfilename(char *prompt, char *buf, int nbuf)
{
	static char temp_file[] = TEMPFILE;
	int cpos = 0;	/* current character position in string */
	int k = 0, c, fd, didtry, iswild = 0;

	char sys_command[255];
	FILE *fp = NULL;
	buf[0] ='\0';

	for (;;) {
		didtry = (k == 0x09);	/* Was last command tab-completion? */
		display_prompt_and_response(prompt, buf);
		k = getch(); /* get a character from the user */

		switch(k) {
		case 0x0a: /* cr, lf */
		case 0x0d:
			if (cpos > 0 && NULL != strchr(buf, '~')) goto do_tab;
			if (fp != NULL) fclose(fp);
			return cpos > 0;

		case 0x07: /* ctrl-g, abort */
			if (fp != NULL) fclose(fp);
			return FALSE;

		case 0x7f: /* del, erase */
		case 0x08: /* backspace */
			if (cpos == 0) continue;
			buf[--cpos] = '\0';
			break;

		case  0x15: /* C-u kill */
			cpos = 0;
			buf[0] = '\0';
			break;

do_tab: 
		case 0x09: /* TAB, complete file name */
			/* scan for a wild card and reset cpos */
			iswild = (strpbrk(buf, "*?") != NULL);
			cpos = 0;

			/* first time retrieval */
			if (! didtry) {
				if (fp != NULL) fclose(fp);
				strcpy(temp_file, TEMPFILE);
				if (-1 == (fd = mkstemp(temp_file)))
					fatal("%s: Failed to create temp file\n");
				snprintf(sys_command, sizeof(sys_command), "echo %s%s >%s 2>&1", buf, iswild ? "" : "*", temp_file);
				(void) ! system(sys_command); /* stop compiler unused result warning */
				fp = fdopen(fd, "r");
				unlink(temp_file);
			}

			/* copy next filename into buf */
			while ((c = getc(fp)) != EOF && c != '\n' && c != ' ')
				if (cpos < nbuf - 1 && c != '*')
					buf[cpos++] = c;

			buf[cpos] = '\0';
			if (c != ' ') rewind(fp);
			break;

		default:
			if (cpos < nbuf - 1) {
				  buf[cpos++] = k;
				  buf[cpos] = '\0';
			}
			break;
		}
	}
}
