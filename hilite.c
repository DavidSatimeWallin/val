/*
 * hilite.c - small state machine for C-style line and block comments.
 *
 * Highlight state lives in buffer_t because display() may resume parsing from
 * an arbitrary page.  set_parse_state() replays bytes before that page, after
 * which parse_text() advances exactly once for each displayed byte.
 *
 * Public Domain, David Satime Wallin 2026, Hugh Barney 2016.
 */

#include "header.h"

#define STATE_LINE_COMMENT   10
#define STATE_BLOCK_COMMENT  11



char_t get_at(buffer_t *bp, point_t pt)
{
	return (*ptr(bp, pt));
}

/* Reconstruct parser state immediately before logical byte offset pt. */
void set_parse_state(buffer_t *bp, point_t pt)
{
	point_t po;

	bp->b_h_state = ID_DEFAULT;
	bp->b_h_next_state = ID_DEFAULT;
	bp->b_h_skip_count = 0;

	for (po = 0; po < pt; po++)
		parse_text(bp, po);
}

/* Return the color role for pt and advance the buffer's parser state. */
int parse_text(buffer_t *bp, point_t pt)
{
	if (bp->b_h_skip_count-- > 0)
		return (bp->b_h_state == STATE_LINE_COMMENT || bp->b_h_state == STATE_BLOCK_COMMENT) ? ID_COMMENT : ID_DEFAULT;

	char_t c_now = get_at(bp, pt);
	char_t c_next = get_at(bp, pt + 1);
	bp->b_h_state = bp->b_h_next_state;

	if (bp->b_h_state == ID_DEFAULT && c_now == '/' && c_next == '*') {
		bp->b_h_skip_count = 1;
		bp->b_h_next_state = bp->b_h_state = STATE_BLOCK_COMMENT;
		return ID_COMMENT;
	}
	if (bp->b_h_state == STATE_BLOCK_COMMENT && c_now == '*' && c_next == '/') {
		bp->b_h_skip_count = 1;
		bp->b_h_next_state = ID_DEFAULT;
		return ID_COMMENT;
	}
	if (bp->b_h_state == ID_DEFAULT && c_now == '/' && c_next == '/') {
		bp->b_h_skip_count = 1;
		bp->b_h_next_state = bp->b_h_state = STATE_LINE_COMMENT;
		return ID_COMMENT;
	}
	if (bp->b_h_state == STATE_LINE_COMMENT && c_now == '\n') {
		bp->b_h_next_state = bp->b_h_state = ID_DEFAULT;
		return ID_DEFAULT;
	}
	if (bp->b_h_state == STATE_LINE_COMMENT || bp->b_h_state == STATE_BLOCK_COMMENT)
		return ID_COMMENT;

	return ID_DEFAULT;
}
