# Makefile fragment for the translator
#
# Copyright (C) 1999 MASS Lab, Seoul National University
#
# See the file LICENSE for usage and redistribution terms and a
# disclaimer of all warranties.
#
# Written by Yoo C. Chung <chungyc@altair.snu.ac.kr>

INCLUDES	+= -I$(srcdir)/translator

JIT_OBJECTS=\
		fast_mem_allocator.o \
		translate.o \
		bytecode_analysis.o \
		cfg_generator.o \
		register_allocator.o \
		AllocStat.o \
		code_sequences.o \
		code_gen.o \
		InstrNode.o \
		CFG.o \
		bit_vector.o \
		pseudo_reg.o \
		reg.o \
		exception_info.o \
		cell_node.o \
		probe.o \
		stat.o \
		method_inlining.o \
		mem_allocator_for_code.o \
		live_analysis.o \
		value_numbering.o \
		loop_opt.o \
		sma.o \
		functions.o \
		translator_driver.o \
		translator_driver_stub.o \
		reg_alloc_util.o \
		plist.o \
		utils.o \
		dynamic_cha.o


OBJECTS		+= $(JIT_OBJECTS)

