/* gap.c, Val Emacs, Public Domain, David Satime Wallin 2026, Hugh Barney, 2016, Derived from: Anthony's Editor January 93 */

#include <sys/stat.h>
#include "header.h"

/* Enlarge gap by n chars, position of gap cannot change */
int growgap(buffer_t *bp, point_t n)
{
	char_t *new;
	point_t buflen, newlen, tail, xgap, xegap;
		
	if (bp->b_buf == NULL) {
		assert(bp->b_gap == NULL && bp->b_egap == NULL && bp->b_ebuf == NULL);
		xgap = xegap = buflen = 0;
	} else {
		assert(bp->b_buf <= bp->b_gap);
		assert(bp->b_gap <= bp->b_egap);
		assert(bp->b_egap <= bp->b_ebuf);
		xgap = bp->b_gap - bp->b_buf;
		xegap = bp->b_egap - bp->b_buf;
		buflen = bp->b_ebuf - bp->b_buf;
	}
    
	/* reduce number of reallocs by growing by a minimum amount */
	n = (n < MIN_GAP_EXPAND ? MIN_GAP_EXPAND : n);
	newlen = buflen + n * sizeof (char_t);

	if (buflen == 0) {
		if (newlen < 0 || MAX_SIZE_T < (unsigned long)newlen)
			fatal("%s: Failed to allocate required memory.\n");
		new = (char_t*) malloc((size_t) newlen);
		if (new == NULL)			
			fatal("%s: Failed to allocate required memory.\n");	/* Cannot edit a file without a buffer. */
	} else {
		if (newlen < 0 || MAX_SIZE_T < (unsigned long)newlen) {
			msg("Failed to allocate required memory");
			return (FALSE);
		}
		new = (char_t*) realloc(bp->b_buf, (size_t) newlen);
		if (new == NULL) {
			msg("Failed to allocate required memory");    /* Report non-fatal error. */
			return (FALSE);
		}
	}

	/* Move the text after the old gap to the end of the larger buffer. */
	tail = buflen - xegap;
	memmove(new + newlen - tail, new + xegap, (size_t)tail);
	bp->b_buf = new;
	bp->b_gap = new + xgap;
	bp->b_egap = new + newlen - tail;
	bp->b_ebuf = new + newlen;

	assert(bp->b_buf < bp->b_ebuf);          /* Buffer must exist. */
	assert(bp->b_buf <= bp->b_gap);
	assert(bp->b_gap < bp->b_egap);          /* Gap must grow only. */
	assert(bp->b_egap <= bp->b_ebuf);
	return (TRUE);
}

point_t movegap(buffer_t *bp, point_t offset)
{
	char_t *p = ptr(bp, offset);
	if (p < bp->b_gap) {
		size_t n = (size_t)(bp->b_gap - p);
		memmove(bp->b_egap - n, p, n);
		bp->b_gap -= n;
		bp->b_egap -= n;
	} else if (p > bp->b_egap) {
		size_t n = (size_t)(p - bp->b_egap);
		memmove(bp->b_gap, bp->b_egap, n);
		bp->b_gap += n;
		bp->b_egap += n;
	}
	assert(bp->b_gap <= bp->b_egap);
	assert(bp->b_buf <= bp->b_gap);
	assert(bp->b_egap <= bp->b_ebuf);
	return (pos(bp, bp->b_egap));
}

/* Given a buffer offset, convert it to a pointer into the buffer */
char_t * ptr(buffer_t *bp, point_t offset)
{
	if (offset < 0)
		return (bp->b_buf);
	return (bp->b_buf+offset + (bp->b_buf + offset < bp->b_gap ? 0 : bp->b_egap-bp->b_gap));
}

/* Given a pointer into the buffer, convert it to a buffer offset */
point_t pos(buffer_t *bp, char_t *cp)
{
	assert(bp->b_buf <= cp && cp <= bp->b_ebuf);
	return (cp - bp->b_buf - (cp < bp->b_egap ? 0 : bp->b_egap - bp->b_gap));
}

int posix_file(char *fn)
{
	if (fn[0] == '_')
		return (FALSE);

	for (; *fn != '\0'; ++fn) {
		if (!isalnum(*fn) && *fn != '.' && *fn != '_' && *fn != '-' && *fn != '/')
			return (FALSE);
	}
	return (TRUE);
}

int save(char *fn)
{
	FILE *fp;
	point_t length;
		
	if (!posix_file(fn)) {
		msg("Not a portable POSIX file name.");
		return (FALSE);
	}
	fp = fopen(fn, "w");
	if (fp == NULL) {
		msg("Failed to open file \"%s\".", fn);
		return (FALSE);
	}
	(void) movegap(curbp, (point_t) 0);
	length = (point_t) (curbp->b_ebuf - curbp->b_egap);
	if (fwrite(curbp->b_egap, sizeof (char), (size_t) length, fp) != (size_t)length) {
		msg("Failed to write file \"%s\".", fn);
		return (FALSE);
	}
	if (fclose(fp) != 0) {
		msg("Failed to close file \"%s\".", fn);
		return (FALSE);
	}
	curbp->b_flags &= ~B_MODIFIED;
	msg("File \"%s\" %ld bytes saved.", fn, pos(curbp, curbp->b_ebuf));
	return (TRUE);
}

int load_file(char *fn)
{
	/* reset the gap, make it the whole buffer */
	curbp->b_gap = curbp->b_buf;
	curbp->b_egap = curbp->b_ebuf;
	top();
	return insert_file(fn, FALSE);
}

/* reads file into buffer at point */
int insert_file(char *fn, int modflag)
{
	FILE *fp;
	size_t len;
	struct stat sb;

	if (stat(fn, &sb) < 0) {
		msg("Failed to find file \"%s\".", fn);
		return (FALSE);
	}
	if (sb.st_size < 0 || MAX_SIZE_T < (unsigned long)sb.st_size) {
		msg("File \"%s\" is too big to load.", fn);
		return (FALSE);
	}
	if ((size_t)(curbp->b_egap - curbp->b_gap) < (size_t)sb.st_size &&
	    !growgap(curbp, (point_t)sb.st_size))
		return (FALSE);
	if ((fp = fopen(fn, "r")) == NULL) {
		msg("Failed to open file \"%s\".", fn);
		return (FALSE);
	}
	curbp->b_point = movegap(curbp, curbp->b_point);
	curbp->b_gap += len = fread(curbp->b_gap, sizeof (char), (size_t) sb.st_size, fp);

	if (fclose(fp) != 0) {
		msg("Failed to close file \"%s\".", fn);
		return (FALSE);
	}
	curbp->b_flags &= (modflag ? B_MODIFIED : ~B_MODIFIED);
	msg("File \"%s\" %ld bytes read.", fn, len);
	return (TRUE);
}

/* find the point for start of line ln */
point_t line_to_point(int ln)
{
	point_t end_p = pos(curbp, curbp->b_ebuf);
	point_t p, start;

	for (p=0, start=0; p < end_p; p++) {
		if ( *(ptr(curbp, p)) == '\n') {
			if (--ln == 0)
				return start;
			if (p + 1 < end_p) 
				start = p + 1;
		}
	}
	return -1;
}

/* scan buffer and fill in curline and lastline */
void get_line_stats(int *curline, int *lastline)
{
	point_t end_p = pos(curbp, curbp->b_ebuf);
	point_t p;
	int line;
    
	*curline = -1;
    
	for (p=0, line=0; p < end_p; p++) {
		line += (*(ptr(curbp,p)) == '\n') ? 1 : 0;
		*lastline = line;
        
		if (*curline == -1 && p == curbp->b_point) {
			*curline = (*(ptr(curbp,p)) == '\n') ? line : line + 1;
		}
	}

	*lastline = *lastline + 1;
	
	if (curbp->b_point == end_p)
		*curline = *lastline;
}
