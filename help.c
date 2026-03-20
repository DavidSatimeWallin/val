/* help.c, Keyboard shortcuts help browser, Val Emacs */

#include "header.h"

void show_keyboard_help(void)
{
    char tmpfile[64];
    snprintf(tmpfile, sizeof(tmpfile), "/tmp/val-help-XXXXXX");
    int fd = mkstemp(tmpfile);
    if (fd < 0) return;
    close(fd);
    
    FILE *fp = fopen(tmpfile, "w");
    if (fp == NULL) return;
    
    for (int i = 0; keymap[i].key_desc != NULL && keymap[i].func != NULL; i++) {
        fputs(keymap[i].key_desc, fp);
        fputc('\n', fp);
    }
    fclose(fp);
    
    buffer_t *bp = find_buffer(tmpfile, TRUE);
    if (bp == NULL) return;
    
    if (bp->b_buf == NULL) {
        if (!growgap(bp, CHUNK)) return;
    }
    
    disassociate_b(curwp);
    curbp = bp;
    associate_b2w(curbp, curwp);
    
    strncpy(bp->b_fname, tmpfile, NAME_MAX);
    bp->b_fname[NAME_MAX] = '\0';
    load_file(tmpfile);
    
    curwp->w_update = TRUE;
}
