# Makefile fragment for Kaffe's bytecode translator
#
# Copyright (C) 1999 MASS Lab, Seoul National University
#
# See the file LICENSE for usage and redistribution terms and a
# disclaimer of all warranties.
#
# Written by Yoo C. Chung <chungyc@altair.snu.ac.kr>

INCLUDES	+= -I$(srcdir)/kaffejit

KAFFE_OBJECTS	=\
		code-analyse.o \
		basecode.o \
		constpool.o \
		funcs.o \
		icode.o \
		labels.o \
		registers.o \
		slots.o \
		machine.o \
		seq.o

OBJECTS		+= $(KAFFE_OBJECTS)
