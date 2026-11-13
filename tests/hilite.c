/* Exercise comment boundaries and page-state reconstruction with a real gap. */
#include "../header.h"

buffer_t *curbp;

void msg(char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	vfprintf(stderr, fmt, args);
	va_end(args);
}

_Noreturn void fatal(char *fmt)
{
	fprintf(stderr, fmt, "test");
	exit(1);
}

void top(void) { curbp->b_point = 0; }

static void check_highlighting(const char *text, const char *roles)
{
	buffer_t bp = {0};
	point_t length = strlen(text);
	assert(strlen(roles) == (size_t)length);
	assert(growgap(&bp, length + 32));
	memcpy(bp.b_gap, text, length);
	bp.b_gap += length;

	/* Split every possible boundary, including the two bytes of a marker. */
	for (point_t gap = 0; gap <= length; gap++) {
		movegap(&bp, gap);
		set_parse_state(&bp, 0);
		for (point_t p = 0; p < length; p++) {
			int expected = roles[p] == 'c' ? ID_COMMENT : ID_DEFAULT;
			assert(parse_text(&bp, p) == expected);
		}
		/* A visible page may begin at any byte, even inside a marker. */
		for (point_t p = 0; p < length; p++) {
			set_parse_state(&bp, p);
			int expected = roles[p] == 'c' ? ID_COMMENT : ID_DEFAULT;
			assert(parse_text(&bp, p) == expected);
		}
	}
	free(bp.b_buf);
}

int main(void)
{
	check_highlighting("// hi\nx\n// y\n", "ccccc...cccc.");
	check_highlighting("/* a\nb */x\n", "ccccccccc..");
	check_highlighting("x // hi\nx /* a\ny\n", ".................");
	check_highlighting(" // x\n\t/* y */\n", "...............");
	check_highlighting("http://x\n\"/*\"\n", "..............");
	check_highlighting("/**///x\n//y\n", "cccc....ccc.");
	check_highlighting("/* x", "cccc");
	puts("syntax-highlighting tests passed");
	return 0;
}
