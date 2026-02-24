/* hlite.c, generic syntax highlighting, Atto Emacs, Hugh Barney, Public Domain, 2016 */

#include "header.h"

#define STATE_LINE_COMMENT   10
#define STATE_BLOCK_COMMENT  11

int state = ID_DEFAULT;
int next_state = ID_DEFAULT;
int skip_count = 0;

char_t get_at(buffer_t *bp, point_t pt)
{
	return (*ptr(bp, pt));
}

void set_parse_state(buffer_t *bp, point_t pt)
{
	register point_t po;

	state = ID_DEFAULT;
	next_state = ID_DEFAULT;
	skip_count = 0;

	for (po = 0; po < pt; po++)
		parse_text(bp, po);
}

int parse_text(buffer_t *bp, point_t pt)
{
	if (skip_count-- > 0)
		return (state == STATE_LINE_COMMENT || state == STATE_BLOCK_COMMENT) ? ID_COMMENT : ID_DEFAULT;

	char_t c_now = get_at(bp, pt);
	char_t c_next = get_at(bp, pt + 1);
	state = next_state;

	if (state == ID_DEFAULT && c_now == '/' && c_next == '*') {
		skip_count = 1;
		next_state = state = STATE_BLOCK_COMMENT;
		return ID_COMMENT;
	}
	if (state == STATE_BLOCK_COMMENT && c_now == '*' && c_next == '/') {
		skip_count = 1;
		next_state = ID_DEFAULT;
		return ID_COMMENT;
	}
	if (state == ID_DEFAULT && c_now == '/' && c_next == '/') {
		skip_count = 1;
		next_state = state = STATE_LINE_COMMENT;
		return ID_COMMENT;
	}
	if (state == STATE_LINE_COMMENT && c_now == '\n') {
		next_state = state = ID_DEFAULT;
		return ID_DEFAULT;
	}
	if (state == STATE_LINE_COMMENT || state == STATE_BLOCK_COMMENT)
		return ID_COMMENT;

	return ID_DEFAULT;
}
