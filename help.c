/*
 * help.c - generate a keyboard-reference buffer from the live keymap.
 *
 * Keeping help text in keymap[] prevents documentation from drifting away from
 * actual bindings.  A temporary file is used so the normal file-loading and
 * buffer-display paths can present the generated reference without a special
 * viewer implementation.
 */

#include "header.h"

/* Materialize key descriptions, switch to that buffer, and request repaint. */
void show_keyboard_help(void)
{
    char tmpfile[64];
    snprintf(tmpfile, sizeof(tmpfile), "/tmp/val-help-XXXXXX");
    int fd = mkstemp(tmpfile);
    if (fd < 0) return;
    close(fd);
    
    FILE *fp = fopen(tmpfile, "w");
    if (fp == NULL) return;

    fputs("Val keyboard help\n\n"
          "C- means Ctrl. esc means press Esc, then the following key.\n"
          "Alt plus that key also works for esc bindings.\n"
          "C-x C-n switches back to the next buffer.\n\n"
          "Key bindings\n\n", fp);
    for (int i = 0; keymap[i].key_desc != NULL && keymap[i].func != NULL; i++) {
        /* Terminal variants share descriptions; show each description once. */
        int duplicate = 0;
        for (int j = 0; j < i; j++) {
            if (strcmp(keymap[j].key_desc, keymap[i].key_desc) == 0) {
                duplicate = 1;
                break;
            }
        }
        if (duplicate) continue;
        fputs(keymap[i].key_desc, fp);
        fputc('\n', fp);
    }
    fputs("\nCompletion in .go and .v files\n\n"
          "Type two identifier characters to see a dim suggestion.\n"
          "Tab accepts it; esc / cycles alternatives.\n"
          "Tab inserts a tab when no suggestion is available.\n"
          "Backspace updates suggestions; movement and C-g dismiss them.\n"
          "Candidates come from keywords, builtins, and open same-language files.\n\n"
          "Highlighting and selection\n\n"
          "// and /* start comments only in column one (no indentation).\n"
          "// ends at the newline; /* continues across lines through */.\n"
          "Markers in the middle of a line do not start comments.\n"
          "Comments are bold blue; function-call names are bold.\n"
          "The current line has a cyan background.\n"
          "C-space sets the mark; movement selects; C-g clears the selection.\n"
          "Selected text uses reverse video; R in the modeline marks a region.\n\n"
          "Home and End\n\n"
          "Common terminal variants move to the beginning/end of the line.\n"
          "C-a and C-e provide the same movement.\n", fp);
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
