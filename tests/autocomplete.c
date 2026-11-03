/* Exercise completion against the real gap buffer and undo implementation. */
#include "../autocomplete.c"

buffer_t *curbp, *bheadp;
static char last_message[TEMPBUF];

void msg(char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	vsnprintf(last_message, sizeof(last_message), fmt, args);
	va_end(args);
}

_Noreturn void fatal(char *fmt)
{
	fprintf(stderr, fmt, "test");
	exit(1);
}

void top(void) { curbp->b_point = 0; }

static void init_buffer(buffer_t *bp, const char *name, const char *text)
{
	memset(bp, 0, sizeof(*bp));
	strcpy(bp->b_fname, name);
	assert(growgap(bp, strlen(text) + 32));
	memcpy(bp->b_gap, text, strlen(text));
	bp->b_gap += strlen(text);
	bp->b_point = strlen(text);
	bp->b_mark = NOMARK;
	undo_init(bp);
}

static void free_buffer(buffer_t *bp)
{
	autocomplete_clear();
	undo_free_all(bp);
	free(bp->b_buf);
}

static void expect_text(buffer_t *bp, const char *text)
{
	assert(pos(bp, bp->b_ebuf) == (point_t)strlen(text));
	for (point_t p = 0; text[p]; p++)
		assert(*ptr(bp, p) == (unsigned char)text[p]);
}

static int contains(const completions_t *set, const char *word)
{
	for (int i = 0; i < set->count; i++)
		if (!strcmp(set->words[i], word))
			return TRUE;
	return FALSE;
}

static void test_candidates(void)
{
	buffer_t current, other, vfile;
	completions_t set;
	init_buffer(&current, "main.go", "myLocal myLocal myPrefix\nmy");
	init_buffer(&other, "other.go", "myOther myPrefix 1myBad émyBad myéBad");
	init_buffer(&vfile, "other.v", "myVOnly");
	curbp = bheadp = &current;
	current.b_next = &other;
	other.b_next = &vfile;
	/* Logical tokens must survive a gap in the middle of an identifier. */
	movegap(&current, 3);
	movegap(&other, 4);
	collect_completions(&set, ".go", "my");
	assert(set.count == 3);
	assert(!strcmp(set.words[0], "myLocal"));
	assert(!strcmp(set.words[1], "myOther"));
	assert(!strcmp(set.words[2], "myPrefix"));
	collect_completions(&set, ".go", "pr");
	assert(contains(&set, "print") && contains(&set, "println"));
	collect_completions(&set, ".go", "fu");
	assert(set.count == 1 && contains(&set, "func"));
	curbp = &vfile;
	collect_completions(&set, ".v", "f");
	assert(contains(&set, "fn") && !contains(&set, "func"));
	collect_completions(&set, ".v", "my");
	assert(set.count == 1 && contains(&set, "myVOnly"));
	free_buffer(&current);
	free_buffer(&other);
	free_buffer(&vfile);
}

static void test_insertion(void)
{
	buffer_t bp;
	init_buffer(&bp, "main.go", "é\nfun()");
	curbp = bheadp = &bp;
	bp.b_point = strlen("é\nfun");
	bp.b_flags = B_OVERWRITE;
	movegap(&bp, 1);
	autocomplete_update();
	assert(!strcmp(autocomplete_suffix(&bp), "c"));
	expect_text(&bp, "é\nfun()");
	assert(!bp.b_undo_head && !(bp.b_flags & B_MODIFIED));
	assert(autocomplete_accept());
	expect_text(&bp, "é\nfunc()");
	assert(!*autocomplete_suffix(&bp));
	assert(bp.b_point == (point_t)strlen("é\nfunc"));
	assert(bp.b_flags & B_MODIFIED);
	undo();
	expect_text(&bp, "é\nfun()");
	redo();
	expect_text(&bp, "é\nfunc()");
	free_buffer(&bp);
	/* Force insertion to grow an exhausted gap. */
	init_buffer(&bp, "main.v", "pri");
	memset(bp.b_gap, ' ', bp.b_egap - bp.b_gap);
	bp.b_gap = bp.b_egap;
	bp.b_point = 3;
	autocomplete_update();
	assert(!strcmp(autocomplete_suffix(&bp), "nt"));
	assert(autocomplete_accept());
	assert(*ptr(&bp, 3) == 'n' && *ptr(&bp, 4) == 't');
	assert(*ptr(&bp, 5) == ' ');
	assert(bp.b_point == 5);
	free_buffer(&bp);
}

static void test_no_edit(void)
{
	const char *texts[] = {"", " ", "f", "123", "éfun", "funé", "unknownXYZ", "func"};
	for (size_t i = 0; i < sizeof(texts) / sizeof(texts[0]); i++) {
		buffer_t bp;
		init_buffer(&bp, "main.go", texts[i]);
		curbp = bheadp = &bp;
		autocomplete_update();
		assert(!*autocomplete_suffix(&bp));
		assert(!autocomplete_accept());
		expect_text(&bp, texts[i]);
		assert(!bp.b_undo_head && !(bp.b_flags & B_MODIFIED));
		free_buffer(&bp);
	}
	buffer_t bp;
	init_buffer(&bp, "main.c", "fun");
	curbp = bheadp = &bp;
	autocomplete_update();
	expect_text(&bp, "fun");
	assert(!*autocomplete_suffix(&bp));
	strcpy(bp.b_fname, "main.go");
	bp.b_point = 2;
	autocomplete_update();
	expect_text(&bp, "fun");
	assert(!*autocomplete_suffix(&bp));
	free_buffer(&bp);
}

static void test_inline_suggestion(void)
{
	buffer_t bp;
	init_buffer(&bp, "main.go", "pri");
	curbp = bheadp = &bp;
	autocomplete_update();
	assert(!strcmp(autocomplete_suffix(&bp), "nt"));
	expect_text(&bp, "pri");
	assert(!bp.b_undo_head && !bp.b_flags);
	autocomplete_clear();
	assert(!*autocomplete_suffix(&bp));
	assert(!autocomplete_accept());
	expect_text(&bp, "pri");
	autocomplete_update();
	autocomplete();
	assert(!strcmp(autocomplete_suffix(&bp), "ntln"));
	expect_text(&bp, "pri");
	assert(autocomplete_accept());
	expect_text(&bp, "println");
	undo();
	expect_text(&bp, "pri");
	/* Cycling wraps without inserting or running a modal input loop. */
	autocomplete_update();
	autocomplete();
	autocomplete();
	assert(!strcmp(autocomplete_suffix(&bp), "nt"));
	/* A suggestion cannot be accepted at another point or in another buffer. */
	bp.b_point--;
	assert(!*autocomplete_suffix(&bp));
	assert(!autocomplete_accept());
	bp.b_point++;
	buffer_t other;
	init_buffer(&other, "other.go", "pri");
	curbp = &other;
	assert(!*autocomplete_suffix(&other));
	assert(!autocomplete_accept());
	free_buffer(&other);
	free_buffer(&bp);
}

static void test_limits(void)
{
	buffer_t bp;
	completions_t set;
	char text[8192];
	int n = 0;
	for (int i = 0; i < MAX_COMPLETIONS + 1; i++)
		n += snprintf(text + n, sizeof(text) - n, "zzWord%d ", i);
	init_buffer(&bp, "main.go", text);
	curbp = bheadp = &bp;
	collect_completions(&set, ".go", "zz");
	assert(set.count == MAX_COMPLETIONS && set.limited);
	free_buffer(&bp);
	memset(text, 'a', COMPLETION_WORD);
	text[COMPLETION_WORD] = '\0';
	init_buffer(&bp, "main.go", text);
	collect_completions(&set, ".go", "aa");
	assert(!set.count);
	autocomplete_update();
	assert(!*autocomplete_suffix(&bp));
	expect_text(&bp, text);
	assert(!bp.b_flags);
	free_buffer(&bp);
}

int main(void)
{
	test_candidates();
	test_insertion();
	test_no_edit();
	test_inline_suggestion();
	test_limits();
	puts("autocomplete tests passed");
	return 0;
}
