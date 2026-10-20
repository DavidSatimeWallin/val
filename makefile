#
# makefile
#
# Based on Anthonys Editor January 93
#
# Public Domain 1991, 1993 by Anthony Howe.  No warranty.
#

CC      ?= cc
CFLAGS  ?= -O -Wall
LDLIBS  ?= -lncursesw

OBJ = command.o display.o gap.o key.o search.o buffer.o replace.o window.o \
      complete.o hilite.o funclist.o gotodef.o diff.o help.o menu.o undo.o main.o

.PHONY: all clean install
all: val

val: $(OBJ)
	$(CC) $(LDFLAGS) -o $@ $(OBJ) $(LDLIBS)

%.o: %.c header.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $<

clean:
	rm -f $(OBJ) val

install: val
	install -m 755 val /usr/local/bin/val

