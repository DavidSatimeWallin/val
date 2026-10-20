# Val architecture

This document describes how the editor fits together and records the invariants
that are easy to miss when changing the C source. The comments beside functions
and structures remain the authoritative local documentation; this file provides
the wider context.

## Design

Val is a single-process, single-threaded terminal editor. Its main loop does
three things in order:

1. render all windows;
2. decode one key sequence or UTF-8 character;
3. invoke one `void(void)` command.

Commands directly update the current buffer and window. This global-state model
is intentional: it keeps command dispatch and the data flow small. Code that
adds asynchronous work or retained callbacks must not assume that the current
buffer remains unchanged between callbacks.

Recoverable failures are shown with `msg()`. `fatal()` restores the terminal and
terminates the process for failures that make editing impossible.

## Source modules

- `main.c`: startup, terminal initialization, global state, and command loop.
- `header.h`: shared data structures, constants, and cross-module interfaces.
- `gap.c`: gap-buffer memory management, file I/O, and line-number conversion.
- `buffer.c`: buffer allocation, ownership, and the global buffer list.
- `window.c`: screen geometry and buffer/window reference management.
- `display.c`: text rendering, wrapping, gutters, attributes, and cursor layout.
- `key.c`: command bindings, terminal escape decoding, and prompt input.
- `command.c`: editing, movement, file, buffer, and shell commands.
- `undo.c`: per-buffer undo and redo stacks.
- `search.c` / `replace.c`: exact byte search and interactive replacement.
- `hilite.c`: stateful recognition of C-style comments.
- `menu.c`: top menu and dropdown event loop.
- `funclist.c`: lightweight source outline and filtering.
- `gotodef.c`: grep-based project definition search.
- `diff.c`: modal Git diff viewer.
- `complete.c`: filename input and completion.
- `help.c`: keyboard-reference buffer generated from the live keymap.

## Text and the gap buffer

`buffer_t` owns one allocation with four important pointers:

```text
b_buf             b_gap      b_egap             b_ebuf
  | existing text | unused gap | existing text     |
```

The logical document is the concatenation of the two text ranges. The gap is
unused capacity at the current edit location. Insertion consumes bytes from the
left edge of the gap; deletion adds bytes to the gap. `movegap()` changes only
the physical layout, never logical text order.

Required invariants are:

```text
b_buf <= b_gap <= b_egap <= b_ebuf
logical size = allocation size - gap size
```

`ptr()` converts a logical byte offset to a physical pointer and skips the gap.
`pos()` performs the inverse conversion. Code outside `gap.c` should use these
helpers rather than deriving physical addresses itself unless it has first
placed the gap and clearly documents why direct access is valid.

The storage unit is a byte. Consequently, points, marks, page boundaries, undo
positions, and search positions are UTF-8 byte offsets. Character-aware code
must use `utf8_size()` or `prev_utf8_char_size()` and must leave point at a UTF-8
boundary.

## Buffers and windows

A buffer owns:

- the gap-buffer allocation;
- filename and display name;
- modification flags;
- syntax-highlighting state;
- undo and redo records;
- a count of windows that reference it.

A window owns a *view* of a buffer: point, mark, visible page, cursor position,
and screen rectangle. Two windows may therefore show different places in the
same buffer. `associate_b2w()` and `disassociate_b()` are the only functions
that should change the buffer reference count. A buffer may be deleted only
when that count is zero.

Rows 0 and 1 are reserved for the menu. Each editor window ends with a modeline,
and the final terminal row is the shared message/prompt line. Screen-layout
changes must preserve these reservations.

## Input and UTF-8

Most terminals send command keys as byte escape sequences. `get_key()` compares
recorded bytes with `keymap[]` until it finds an exact command, a possible
prefix, or ordinary text.

UTF-8 input also arrives as multiple bytes. `get_key()` collects all continuation
bytes before returning text to `insert()`. This is important: rendering between
individual bytes would expose an invalid partial sequence to ncurses. The input
pointer refers to static storage and is valid only until the next `get_key()`
call.

`keymap[]` is also the source of keyboard help. A new binding should include an
accurate human-readable description because `help.c` displays it verbatim.

## Display

Display code translates between two coordinate systems:

- logical UTF-8 byte offsets in buffers;
- terminal rows and display-cell columns.

A multibyte character can occupy one, two, or occasionally zero terminal cells.
The renderer uses `wcwidth()` for this conversion. Line wrapping and cursor
placement must use cell widths, while traversal must use byte lengths.

`update_display()` synchronizes the current buffer with every affected window.
In a multi-window layout it delays the final refresh until all views are
coherent. Commands should normally mutate state and return rather than adding
new refresh calls.

## Undo and redo

Each undo record stores an edit type, logical position, byte length, and a copy
of the affected bytes. An insertion is undone by deleting its range; a deletion
is undone by reinserting its saved text. Applied records move between the undo
and redo stacks.

Adjacent typing and adjacent backwards deletion are coalesced. A new edit clears
the redo stack. Internal restoration sets `b_no_undo` so applying undo does not
record another user edit.

## Modal tools

The menu, function list, definition chooser, and diff viewer temporarily run
their own input loops. They must restore the normal editor display with
`redraw()` before returning. These tools should reuse normal commands and
navigation helpers where practical instead of creating a second editing model.

## Extending Val

When adding a command:

1. implement a `void command(void)` function in the module that owns the behavior;
2. declare it in `header.h` if another module calls it;
3. add its terminal sequence and description to `keymap[]`;
4. add it to the menu only if it is useful there;
5. preserve gap-buffer and UTF-8 boundary invariants;
6. report recoverable errors with `msg()`;
7. rebuild with strict warnings and test through a terminal.

Recommended verification:

```sh
make clean
make CFLAGS='-O2 -Wall -Wextra -Wpedantic'
scan-build --status-bugs make clean all
```

Interactive changes should also exercise UTF-8 input, cursor movement, deletion,
undo/redo, menu activation, and saving, since those paths cross most core
invariants.
