# Val

A small terminal text editor written in C with Emacs-style key bindings,
undo/redo, multiple buffers and windows, Go/V inline completion, and Git tools.

Originally based on [Atto](https://github.com/hughbarney/atto) by Hugh Barney, which itself derives from Anthony Howe's public domain editor. Val Emacs is maintained by David Satime Wallin.

## Disclaimer

This software is provided as-is, with no warranty of any kind. Use this editor at your own risk. The authors are not responsible for any data loss or damage resulting from its use. Always keep backups of important files.

## Features

- **Menu bar** -- keyboard-driven access to common commands (F10 or `C-x m`)
- **Multiple buffers** -- open, switch between, and close files independently
- **Multiple windows** -- split the screen and view different buffers (or the same buffer) side by side
- **Visual region highlighting** -- the selected region is displayed in reverse video, giving clear visual feedback when the mark is active
- **Cut, copy, and paste** -- mark a region, cut or copy it, and yank it back anywhere
- **Search** -- forward and backward text search with wrap-around
- **Search and replace** -- interactive query-replace
- **Syntax highlighting** -- comments starting with `//` or `/*` in column one displayed in bold blue (block comments continue through `*/`); function calls displayed in bold
- **Relative line numbers** -- gutter shows absolute line number on the current line and relative distances on all others
- **UTF-8 support** -- edit files containing multi-byte and wide characters
- **Filename completion** -- tab-complete file paths at prompts
- **Go and V completion** -- automatic inline suggestions for keywords, common builtins, and identifiers from open buffers; Tab accepts
- **fzf integration** -- fuzzy-find and open files with `M-.` (requires `/usr/bin/fzf`)
- **Shell commands** -- run a shell command and view its output with `M--`
- **Git diff viewer** -- browse the current file's unstaged changes with `M-d`; Enter jumps to the selected source line
- **Go to definition** -- jump to a C function definition with `M-,`
- **Function list** -- filter and jump to functions in C/C++, Go, V, Python, Ruby, PHP, Zig, Crystal, and Rust files with `M-l`
- **Jump command** -- jump to an absolute line or move up/down by a relative number of lines
- **Overwrite mode** -- toggle between insert and overwrite with the Ins key
- **Buffer-gap architecture** -- efficient internal representation that keeps the entire codebase small and fast

## Building

Requires `libncursesw`:

    sudo apt-get install libncurses5-dev

Then:

    make
    sudo make install

Run `./val path/to/file` from the build directory, or `val path/to/file` after
installing. Run without a filename to open a scratch buffer.

## Tests

Run `make test` for completion and syntax-highlighting regression tests. The
highlighting tests cover comment boundaries, gaps between marker bytes, and
parser state when scrolling into a comment.

## Developer Documentation

See [`docs/architecture.md`](docs/architecture.md) for the module map, data
ownership rules, gap-buffer invariants, UTF-8 coordinate model, display flow,
and guidance for adding commands safely. The source files also document their
module responsibilities and non-obvious invariants directly beside the code.

## Key Bindings

`C-` means Ctrl. `M-` means Alt, or press Esc followed by the key. For example,
`M-/` is Alt+/ or Esc then /. Open the built-in reference with `C-x h`.

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

Home and End accept common terminal variants, including the sequences ending
in `1~`/`4~` and `7~`/`8~`. `C-a` and `C-e` provide the same movement.

### Editing

	C-_ / C-x u      undo
	M-_              redo
	Tab              accept inline completion (otherwise insert a tab)
	M-/              cycle inline completions
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
    Esc Esc          show version
    C-x C-c          exit (prompts if unsaved buffers exist)

## Highlighting

Comment highlighting starts only when `//` or `/*` is in column one, with no
leading spaces or tabs. `//` continues to the end of the line; `/*` continues
across lines until `*/`, including when scrolling into the middle of the block.
Markers inside a line, such as those in URLs, strings, or trailing comments,
do not start comment highlighting.

Comments appear in bold blue and function-call names in bold. The current line
has a cyan background; an active selection appears in reverse video. `C-Space`
sets the mark, movement extends the selection, and `C-g` clears it. The modeline
shows `R` while a nonempty region is active.

## Go and V completion

In a `.go` or `.v` file, suggestions appear automatically as dim text at the
cursor after typing two identifier characters. Keep typing to refine the
suggestion, press Tab to accept, or use `M-/` (Alt+/, or Esc then /) to cycle
alternatives. The Edit menu also offers Next Completion. Backspace updates the
suggestion; `C-g`, movement, and other commands dismiss it. Enter inserts a
newline. Tab inserts a tab when there is no suggestion.

Suggestions only affect the display until accepted; they do not modify the
buffer, its saved contents, or undo history. The preview shifts following text
on the current screen row and clips at the right edge without wrapping.

Completion uses keywords, common builtins, and words from open files with the
same extension, including unsaved edits. Matching is case-sensitive and works
at the end of ASCII identifiers, up to 127 bytes. Up to 256 unique matches are
available, in alphabetical order. Longer words and identifiers containing
non-ASCII characters are skipped.

This is lexical completion: words in comments and strings can appear, and
suggestions are not filtered by scope or receiver type. It does not load
unopened project files, resolve imports, or add function arguments. Accepted
text participates in normal undo/redo and is inserted even in overwrite mode.
