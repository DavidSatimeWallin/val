# Val

A tiny terminal text editor with Emacs-style key bindings.

Originally based on [Atto](https://github.com/hughbarney/atto) by Hugh Barney, which itself derives from Anthony Howe's public domain editor.

## Disclaimer

This software is provided as-is, with no warranty of any kind. Use this editor at your own risk. The authors are not responsible for any data loss or damage resulting from its use. Always keep backups of important files.

## Features

- **Menu bar** -- interactive top menu with all keyboard shortcuts (F10 or `C-x m`)
- **Multiple buffers** -- open, switch between, and close files independently
- **Multiple windows** -- split the screen and view different buffers (or the same buffer) side by side
- **Visual region highlighting** -- the selected region is displayed in reverse video, giving clear visual feedback when the mark is active
- **Cut, copy, and paste** -- mark a region, cut or copy it, and yank it back anywhere
- **Search** -- incremental forward and backward search with wrap-around
- **Search and replace** -- interactive query-replace
- **Syntax highlighting** -- `//` and `/* */` comments displayed in bold blue; function calls displayed in bold
- **Relative line numbers** -- gutter shows absolute line number on the current line and relative distances on all others
- **UTF-8 support** -- edit files containing multi-byte and wide characters
- **Filename completion** -- tab-complete file paths at prompts
- **fzf integration** -- fuzzy-find and open files with `M-.` (requires `/usr/bin/fzf`)
- **Shell commands** -- run a shell command and view its output with `M--`
- **Git diff viewer** -- browse git diff output with keyboard navigation using `M-d`
- **Go to definition** -- jump to a C function definition with `M-,`
- **Function list** -- browse and jump to functions in the current buffer with `M-l`
- **Jump command** -- jump to an absolute line or move up/down by a relative number of lines
- **Overwrite mode** -- toggle between insert and overwrite with the Ins key
- **Buffer-gap architecture** -- efficient internal representation that keeps the entire codebase small and fast

## Building

Requires `libncursesw`:

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

	C-_ / C-x u      undo
	M-_              redo
	C-d / Del        delete character under cursor
	C-h / Backspace  delete character to the left
	C-k              kill to end of line
	C-Space          set mark (region highlighted in reverse video)
	C-w              kill region (cut)
	M-w              copy region
	C-y              yank (paste)
	C-g              deactivate mark (clear region)
	Ins              toggle overwrite mode

### Search and Replace

    C-s              search forward
    C-r              search backward
    M-r              query replace

### Files and Buffers

    C-x C-f          find file (open into new buffer)
    M-.              find file with fzf
    M--              run shell command
    C-x C-s          save buffer
    C-x C-w          write buffer (prompt for filename)
    C-x i            insert file at point
    C-x C-n / C-x n  next buffer
    C-x k            kill buffer

### Windows

    C-x 2            split window
    C-x o            switch to other window
    C-x 1            delete other windows

### Help and Utilities

    C-x h            keyboard shortcuts help browser
    C-x r i          indent-to-tabs conversion
    M-d              git diff viewer
    M-,              go to definition
    M-l              function list
    F10 / C-x m      menu bar

### Other

    C-x =            show cursor position
    C-l              refresh display
    M-M              show version
    C-x C-c          exit (prompts if unsaved buffers exist)
