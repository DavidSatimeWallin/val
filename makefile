#
# makefile
#
# Based on Anthonys Editor January 93
#
# Public Domain 1991, 1993 by Anthony Howe.  No warranty.
#

# Callers may override toolchain variables, for example:
#   make CC=clang CFLAGS='-O2 -Wall -Wextra'
CC      ?= cc
CFLAGS  ?= -O -Wall
LDLIBS  ?= -lncursesw

# Keep the complete link set explicit: adding a module requires one obvious edit.
OBJ = command.o display.o gap.o key.o search.o buffer.o replace.o window.o \
      complete.o hilite.o funclist.o gotodef.o diff.o help.o menu.o undo.o main.o

.PHONY: all clean install
all: val

val: $(OBJ)
	$(CC) $(LDFLAGS) -o $@ $(OBJ) $(LDLIBS)

# All modules share header.h, so an interface change rebuilds every consumer.
%.o: %.c header.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $<

clean:
	rm -f $(OBJ) val

# Copy rather than move val so the local build remains available after install.
install: val
	install -m 755 val /usr/local/bin/val

