/* undo.c, Val Emacs, Public Domain, 2026 */

#include "header.h"

#define UNDO_INSERT 0
#define UNDO_DELETE 1

typedef struct undo_rec_t {
	struct undo_rec_t *next;
	point_t point;
	int len;
	int type;
	char_t *text;
} undo_rec_t;

static undo_rec_t *undo_alloc(point_t point, int len, int type, char_t *text)
{
	undo_rec_t *r = malloc(sizeof(undo_rec_t));
	if (!r) return NULL;
	r->point = point;
	r->len = len;
	r->type = type;
	r->text = malloc(len);
	if (!r->text) { free(r); return NULL; }
	memcpy(r->text, text, len);
	r->next = NULL;
	return r;
}

static void undo_free(undo_rec_t *r)
{
	if (!r) return;
	free(r->text);
	free(r);
}

static void undo_clear(buffer_t *bp)
{
	undo_rec_t *r;
	while ((r = bp->b_undo_head) != NULL) {
		bp->b_undo_head = r->next;
		undo_free(r);
	}
}

static void redo_clear(buffer_t *bp)
{
	undo_rec_t *r;
	while ((r = bp->b_redo_head) != NULL) {
		bp->b_redo_head = r->next;
		undo_free(r);
	}
}

void undo_init(buffer_t *bp)
{
	bp->b_undo_head = NULL;
	bp->b_redo_head = NULL;
	bp->b_no_undo = 0;
}

void undo_free_all(buffer_t *bp)
{
	undo_clear(bp);
	redo_clear(bp);
}

static void undo_commit(buffer_t *bp, undo_rec_t *r)
{
	if (!r) return;
	redo_clear(bp);
	r->next = bp->b_undo_head;
	bp->b_undo_head = r;
}

static void undo_insert_at(buffer_t *bp, point_t p, char_t *text, int len)
{
	if (len > bp->b_egap - bp->b_gap)
		growgap(bp, len);
	bp->b_point = movegap(bp, p);
	memcpy(bp->b_gap, text, len);
	bp->b_gap += len;
	bp->b_point = pos(bp, bp->b_egap);
}

static void undo_delete_at(buffer_t *bp, point_t p, int len)
{
	bp->b_point = movegap(bp, p);
	bp->b_egap += len;
	bp->b_point = pos(bp, bp->b_egap);
}

void record_undo(buffer_t *bp, point_t point, int len, int type, char_t *text)
{
	if (bp->b_no_undo || len == 0)
		return;

	undo_rec_t *head = bp->b_undo_head;

	if (head && head->type == type && !bp->b_redo_head) {
		if (type == UNDO_INSERT && head->point + head->len == point) {
			char_t *new = realloc(head->text, head->len + len);
			if (!new) goto append;
			memcpy(new + head->len, text, len);
			head->text = new;
			head->len += len;
			return;
		}
		if (type == UNDO_DELETE && point + len == head->point) {
			char_t *new = realloc(head->text, head->len + len);
			if (!new) goto append;
			memmove(new + len, new, head->len);
			memcpy(new, text, len);
			head->text = new;
			head->point = point;
			head->len += len;
			return;
		}
	}

append:
	undo_commit(bp, undo_alloc(point, len, type, text));
}

void undo(void)
{
	buffer_t *bp = curbp;
	undo_rec_t *r = bp->b_undo_head;
	if (!r) { msg("No further undo information"); return; }

	bp->b_undo_head = r->next;
	bp->b_no_undo = 1;

	if (r->type == UNDO_INSERT)
		undo_delete_at(bp, r->point, r->len);
	else
		undo_insert_at(bp, r->point, r->text, r->len);

	bp->b_no_undo = 0;
	bp->b_flags |= B_MODIFIED;

	r->next = bp->b_redo_head;
	bp->b_redo_head = r;

	msg("Undo");
}

void redo(void)
{
	buffer_t *bp = curbp;
	undo_rec_t *r = bp->b_redo_head;
	if (!r) { msg("No further redo information"); return; }

	bp->b_redo_head = r->next;
	bp->b_no_undo = 1;

	if (r->type == UNDO_INSERT)
		undo_insert_at(bp, r->point, r->text, r->len);
	else
		undo_delete_at(bp, r->point, r->len);

	bp->b_no_undo = 0;
	bp->b_flags |= B_MODIFIED;

	r->next = bp->b_undo_head;
	bp->b_undo_head = r;

	msg("Redo");
}
