/*
 * autocomplete.c - automatic inline completion for Go and V buffers.
 *
 * Candidates are keywords, common builtins, and ASCII identifiers from open
 * buffers of the same language. Pending candidates are display-only until Tab
 * accepts one; the command loop clears them before unrelated commands.
 * Public Domain, 2026.
 */

#include "header.h"

#define MAX_COMPLETIONS 256
#define COMPLETION_WORD 128

typedef struct {
	char words[MAX_COMPLETIONS][COMPLETION_WORD];
	int count;
	int limited;
} completions_t;

/* Sources: https://go.dev/ref/spec#Keywords and #Predeclared_identifiers */
static const char go_words[] =
	"break case chan const continue default defer else fallthrough for func "
	"go goto if import interface map package range return select struct switch "
	"type var any bool byte comparable complex64 complex128 error float32 "
	"float64 int int8 int16 int32 int64 rune string uint uint8 uint16 uint32 "
	"uint64 uintptr true false iota nil append cap clear close complex copy "
	"delete imag len make max min new panic print println real recover";

/* Sources: vlang/v vlib/v/token/token.v and https://docs.vlang.io/ */
static const char v_words[] =
	"as asm assert atomic break const continue defer dump else enum false fn "
	"for __global go goto if import in interface is isreftype _likely_ lock "
	"match module mut nil none __offsetof or pub return rlock select shared "
	"sizeof spawn static struct true type typeof union _unlikely_ unsafe "
	"volatile bool byte char f32 f64 i8 i16 int i64 isize u8 u16 u32 u64 "
	"usize rune string voidptr print println eprint eprintln panic exit";

/* Treat UTF-8 bytes as part of a token so an ASCII suffix is never completed. */
static int word_byte(unsigned char c)
{
	return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
	       (c >= '0' && c <= '9') || c == '_' || c >= 0x80;
}

static int ascii_identifier(const char *word)
{
	if (!word[0] || (word[0] >= '0' && word[0] <= '9'))
		return FALSE;
	for (; *word; word++)
		if ((unsigned char)*word >= 0x80 || !word_byte(*word))
			return FALSE;
	return TRUE;
}

static const char *completion_language(buffer_t *bp)
{
	const char *ext = strrchr(bp->b_fname, '.');
	if (ext && (!strcmp(ext, ".go") || !strcmp(ext, ".v")))
		return ext;
	return NULL;
}

static void add_completion(completions_t *set, const char *word,
			   const char *prefix)
{
	size_t len = strlen(prefix);
	if (strncmp(word, prefix, len) || strlen(word) <= len)
		return;
	for (int i = 0; i < set->count; i++)
		if (!strcmp(set->words[i], word))
			return;
	if (set->count == MAX_COMPLETIONS) {
		set->limited = TRUE;
		return;
	}
	strcpy(set->words[set->count++], word);
}

static void scan_completion_buffer(completions_t *set, buffer_t *bp,
				   const char *prefix)
{
	point_t end = bp->b_buf ? pos(bp, bp->b_ebuf) : 0;
	point_t p = 0;
	while (p < end) {
		char word[COMPLETION_WORD];
		int n = 0;
		if (!word_byte(*ptr(bp, p))) {
			p++;
			continue;
		}
		point_t start = p;
		while (p < end && word_byte(*ptr(bp, p))) {
			if (n < COMPLETION_WORD - 1)
				word[n++] = *ptr(bp, p);
			p++;
		}
		word[n] = '\0';
		if (p - start < COMPLETION_WORD && ascii_identifier(word))
			add_completion(set, word, prefix);
	}
}

static int compare_completions(const void *a, const void *b)
{
	return strcmp(a, b);
}

static void collect_completions(completions_t *set, const char *language,
				const char *prefix)
{
	char builtins[sizeof(go_words) + sizeof(v_words)];
	char *word, *saveptr;
	memset(set, 0, sizeof(*set));
	strcpy(builtins, !strcmp(language, ".go") ? go_words : v_words);
	for (word = strtok_r(builtins, " ", &saveptr); word;
	     word = strtok_r(NULL, " ", &saveptr))
		add_completion(set, word, prefix);
	/* Prefer current-buffer words when the candidate limit is reached. */
	scan_completion_buffer(set, curbp, prefix);
	for (buffer_t *bp = bheadp; bp; bp = bp->b_next) {
		const char *other = completion_language(bp);
		if (bp != curbp && other && !strcmp(other, language))
			scan_completion_buffer(set, bp, prefix);
	}
	qsort(set->words, set->count, sizeof(set->words[0]), compare_completions);
}

/* One pending suggestion belongs to the buffer and point where it was built. */
static completions_t pending;
static buffer_t *pending_buffer;
static point_t pending_point;
static size_t prefix_length;
static int selected;

void autocomplete_clear(void)
{
	pending_buffer = NULL;
	pending.count = 0;
}

/* Return display-only ASCII text; never move point or modify the gap buffer. */
const char *autocomplete_suffix(buffer_t *bp)
{
	if (bp != pending_buffer || !pending.count || bp->b_point != pending_point)
		return "";
	return pending.words[selected] + prefix_length;
}

/* Called after typing/backspacing, never during rendering or prompt input. */
void autocomplete_update(void)
{
	const char *language = completion_language(curbp);
	point_t point = curbp->b_point, start = point;
	char prefix[COMPLETION_WORD];
	autocomplete_clear();
	if (!language || !curbp->b_buf)
		return;
	point_t end = pos(curbp, curbp->b_ebuf);
	if (point < end && word_byte(*ptr(curbp, point)))
		return;
	while (start > 0 && word_byte(*ptr(curbp, start - 1)))
		start--;
	/* Two characters avoid suggestions after every single-letter variable. */
	if (point - start < 2 || point - start >= COMPLETION_WORD)
		return;
	for (point_t p = start; p < point; p++)
		prefix[p - start] = *ptr(curbp, p);
	prefix[point - start] = '\0';
	if (!ascii_identifier(prefix))
		return;
	collect_completions(&pending, language, prefix);
	pending_buffer = curbp;
	pending_point = point;
	prefix_length = strlen(prefix);
	selected = 0;
}

/* Optional M-/ cycles the already visible candidates without a modal prompt. */
void autocomplete(void)
{
	if (*autocomplete_suffix(curbp))
		selected = (selected + 1) % pending.count;
	else {
		autocomplete_update();
		if (!*autocomplete_suffix(curbp))
			msg("Completion: no suggestions at point");
	}
}

/* Return TRUE when Tab was consumed, including an allocation failure. */
int autocomplete_accept(void)
{
	const char *suffix = autocomplete_suffix(curbp);
	if (!*suffix)
		return FALSE;
	point_t point = curbp->b_point;
	int len = strlen(suffix);
	if (curbp->b_egap - curbp->b_gap < len && !growgap(curbp, len)) {
		autocomplete_clear();
		return TRUE;
	}
	movegap(curbp, point);
	memcpy(curbp->b_gap, suffix, len);
	curbp->b_gap += len;
	curbp->b_point = point + len;
	record_undo(curbp, point, len, 0, (char_t *)suffix);
	curbp->b_flags |= B_MODIFIED;
	if (curbp->b_point == pos(curbp, curbp->b_ebuf) &&
	    curbp->b_point >= curbp->b_epage)
		curbp->b_reframe = 1;
	autocomplete_clear();
	return TRUE;
}
