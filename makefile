#
# makefile
#
# Based on Anthonys Editor January 93
#
# Public Domain 1991, 1993 by Anthony Howe.  No warranty.
#

CC      = cc
CFLAGS  = -O -Wall

LD      = cc
LDFLAGS =
LIBS    = -lncursesw

CP      = cp
MV      = mv
RM      = rm

E       =
O       = .o

OBJ     = command$(O) display$(O) gap$(O) key$(O) search$(O) buffer$(O) replace$(O) window$(O) complete$(O) hilite$(O) funclist$(O) gotodef$(O) diff$(O) help$(O) menu$(O) undo$(O) main$(O)

val$(E) : $(OBJ)
	$(LD) $(LDFLAGS) -o val$(E) $(OBJ) $(LIBS)

%.o: %.c header.h
	$(CC) $(CFLAGS) -c $<

clean:
	-$(RM) $(OBJ) val$(E)

install:
	-$(MV) val$(E) /usr/local/bin/

