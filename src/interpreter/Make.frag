# Makefile fragment for the interpreter
#
# Copyright (C) 1999 MASS Lab, Seoul National University
#
# See the file LICENSE for usage and redistribution terms and a
# disclaimer of all warranties.
#
# Written by Yoo C. Chung <chungyc@altair.snu.ac.kr>

INCLUDES	+= -I$(srcdir)/interpreter

INTRP_OBJECTS=\
		interpreter.o \
		intrp-resolve.o \
		intrp-support.o \
		intrp-exception.o

OBJECTS		+= $(INTRP_OBJECTS)

interpreter.o: interpreter.S offsets.S
	$(GASP) -u $< | $(AS) -Av9 -o interpreter.o

offsets.S: print_offsets
	./print_offsets > offsets.S

print_offsets: $(srcdir)/../util/print_offsets.c
	$(CC) $(CFLAGS) $(EXTRA_CFLAGS) $(INCLUDES) -o $@ $<
