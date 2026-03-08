# Val

A tiny terminal text editor with Emacs-style key bindings, written in under 3000 lines of C.

Originally based on [Atto](https://github.com/hughbarney/atto) by Hugh Barney, which itself derives from Anthony Howe's public domain editor.

## Disclaimer

This software is provided as-is, with no warranty of any kind. Use this editor at your own risk. The authors are not responsible for any data loss or damage resulting from its use. Always keep backups of important files.

## Features

- **Multiple buffers** -- open, switch between, and close files independently
- **Multiple windows** -- split the screen and view different buffers (or the same buffer) side by side
- **Cut, copy, and paste** -- mark a region, cut or copy it, and yank it back anywhere
- **Search** -- incremental forward and backward search with wrap-around
- **Search and replace** -- interactive query-replace
- **Syntax highlighting** -- `//` and `/* */` comments displayed in bold blue; function calls displayed in bold
- **Relative line numbers** -- gutter shows absolute line number on the current line and relative distances on all others
- **UTF-8 support** -- edit files containing multi-byte and wide characters
- **Filename completion** -- tab-complete file paths at prompts
- **fzf integration** -- fuzzy-find and open files with `C-x z` (requires `/usr/bin/fzf`)
- **Shell commands** -- run a shell command and view its output without leaving the editor
- **Go to definition** -- jump to a C function definition with `Esc ,`
- **Function list** -- browse and jump to functions in the current buffer with `Esc l`
- **Jump command** -- jump to an absolute line or move up/down by a relative number of lines
- **Overwrite mode** -- toggle between insert and overwrite with the Ins key
- **Buffer-gap architecture** -- efficient internal representation that keeps the entire codebase small and fast

## Building

Requires `libncursesw`. On Debian/Ubuntu:

    sudo apt-get install libncurses5-dev

Then:

    make
    sudo make install

## Key Bindings

### Movement

    C-b / Left       backward character
    C-f / Right      forward character
    C-p / Up         previous line
    C-n / Down       next line
    C-a / Home       beginning of line
    C-e / End        end of line
    M-b              backward word
    M-f              forward word
    C-v / PgDn       page down
    M-v / PgUp       page up
    M-<              beginning of buffer
    M->              end of buffer
    M-g              go to line
    M-j              jump (absolute, relative up/down)

### Editing

    C-d / Del        delete character under cursor
    C-h / Backspace  delete character to the left
    C-k              kill to end of line
    C-Space          set mark
    C-w              kill region (cut)
    M-w              copy region
    C-y              yank (paste)
    Ins              toggle overwrite mode

### Search and Replace

    C-s              search forward
    C-r              search backward
    M-r              query replace

### Files and Buffers

    C-x C-f          find file (open into new buffer)
    C-x z            find file with fzf
    C-x C-s          save buffer
    C-x C-w          write buffer (prompt for filename)
    C-x i            insert file at point
    C-x C-n / C-x n  next buffer
    C-x k            kill buffer

### Windows

    C-x 2            split window
    C-x o            switch to other window
    C-x 1            delete other windows

### Other

    M-.              shell command
    Esc ,            go to definition
    Esc l            function list
    C-x =            show cursor position
    C-l              refresh display
    Esc Esc          show version
    C-x C-c          exit (prompts if unsaved buffers exist)
