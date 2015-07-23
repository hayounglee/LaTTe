/* reg_alloc_util.c
   
   utility functions for register allocation
   
   Written by: Byung-Sun Yang <scdoner@altair.snu.ac.kr>
   
   Copyright (C) 1999 MASS Lab., Seoul, Korea.
   
   See the file LICENSE for usage and redistribution terms and a
   disclaimer of all warranties.
 */

#include "reg_alloc_util.h"

InstrNode *
RegAlloc_generate_copy(InstrNode *instr, int type, int from_r, int to_r)
{
    InstrNode * new_instr = NULL;

    assert(Reg_IsHW(from_r));
    assert(Reg_IsHW(to_r));

    switch(type) {
      case T_INT: case T_REF: case T_LONG: case T_IVOID:
        new_instr = create_format6_instruction(ADD, to_r, from_r, g0, -1);
        CFG_InsertInstrAsSinglePred(cfg, instr, new_instr);
        break;
      case T_FLOAT:
        assert(Reg_GetType(from_r) == T_FLOAT &&
                Reg_GetType(to_r)   == T_FLOAT);
        new_instr = create_format6_instruction(FMOVS , to_r, g0, from_r, -1);
        CFG_InsertInstrAsSinglePred(cfg, instr, new_instr);
        break;
      case T_DOUBLE:
        new_instr = create_format6_instruction(FMOVD, to_r, g0, from_r, -1);
        CFG_InsertInstrAsSinglePred(cfg, instr, new_instr);
        break;
      case T_CC:
        assert(0);
        break;
      case T_FVOID:
        assert(0 && "T_FVOID never be used.");
      default:
        assert(0);
    }

    if (Instr_IsVisited(instr)) Instr_MarkVisited(new_instr);

    return new_instr;
}

InstrNode *
RegAlloc_copy_register(AllocStat *h, InstrNode *instr, int from_r, int to_r)
{
    int v = AllocStat_FindRMap(h, from_r, 0);

    assert(Var_IsVariable(v));
    assert(Reg_IsHW(from_r));
    assert(Reg_IsHW(to_r));

    switch(Var_GetType(v)) {
      case T_INT: case T_REF: case T_LONG: case T_IVOID: case T_FLOAT:
        AllocStat_ChangeReg(h, from_r, to_r);
        break;
      case T_DOUBLE:
        /* copy from_r->to_r */
        AllocStat_ChangeReg(h, from_r, to_r);
        /* The same treatement of (from_r+1) like following is wrong 
           since AllocStat_ChangeReg() looks for the map from which (from_r+1)
           is mapped */
        /* AllocStat_ChangeReg(h, from_r+1, to_r+1); */
        break;
      case T_CC:
        assert(0);
        break;
      case T_FVOID:
        assert(0 && "T_FVOID never used.");
      default:
        assert(0);
    }

    return RegAlloc_generate_copy(instr, Var_GetType(v), from_r, to_r);
}

int 
RegAlloc_get_spill_offset(Reg r)
{
    int offset = -12 - 4 * (r - Reg_number);
    assert(Reg_IsSpill(r));
    return offset;
}


static
InstrNode *
RegAlloc_generate_spill_to_stack(InstrNode *instr, int type, int from_r, int offset)
{
    InstrNode * new_instr = NULL;

    assert(Reg_IsRegister(from_r));

    switch(type) {
      case T_INT: case T_REF: case T_LONG: case T_IVOID:
        assert(Reg_GetType(from_r) == T_INT);
        new_instr = create_format8_instruction(ST, from_r, fp, offset, -1);
        CFG_InsertInstrAsSinglePred(cfg, instr, new_instr);
        break;
      case T_FLOAT:
        if (Reg_GetType(from_r) == T_FLOAT) {
            new_instr = create_format8_instruction(STF, from_r, fp, offset, -1);
        } else {
            new_instr = create_format8_instruction(ST, from_r, fp, offset, -1);
        }
        CFG_InsertInstrAsSinglePred(cfg, instr, new_instr);
        break;
      case T_DOUBLE:
        assert((offset + 4) % 8 == 0);  /* double word align */

        new_instr = 
            create_format8_instruction(STDF, from_r, fp, offset - 4, -1);
        CFG_InsertInstrAsSinglePred(cfg, instr, new_instr);
        break;
      case T_CC:
        assert(0);
        break;
      default:
        assert(0);
    }

    if (Instr_IsVisited(instr)) Instr_MarkVisited(new_instr);
    return new_instr;
}


InstrNode *
RegAlloc_generate_spill(InstrNode *instr, int type, int from_r, int to_r)
{
    return RegAlloc_generate_spill_to_stack(instr, type, from_r, RegAlloc_get_spill_offset(to_r));
}


InstrNode *
RegAlloc_spill_register(AllocStat *h, InstrNode *instr, int from_r)
{
    int to_r;      // spill register
    int v; 

    assert(AllocStat_GetRefCount(h, from_r) > 0);

    v = AllocStat_FindRMap(h, from_r, 0);
    to_r = AllocStat_FindFreeSpill(h, Var_GetType(v));

    assert(Reg_IsRegister(from_r));
    assert(Var_IsVariable(v));
    assert(Reg_IsSpill(to_r));

    switch(Var_GetType(v)) {
      case T_INT: case T_REF: case T_LONG: case T_IVOID:
        assert(AllocStat_GetRefCount(h, to_r) == 0);

        AllocStat_ChangeReg(h, from_r, to_r);
        break;
      case T_FLOAT:
        assert(AllocStat_GetRefCount(h, to_r) == 0);        

        AllocStat_ChangeReg(h, from_r, to_r);
        break;
      case T_DOUBLE:
        assert(AllocStat_GetRefCount(h, to_r) == 0);
        assert(AllocStat_GetRefCount(h, to_r + 1) == 0);

        AllocStat_ChangeReg(h, from_r, to_r);
        break;
      case T_CC:
        assert(0);
        break;
      default:
        assert(0);
    }

    return RegAlloc_generate_spill(instr, Var_GetType(v), from_r, to_r);
}

static
InstrNode *
RegAlloc_generate_promotion_from_stack(InstrNode *instr, int type, int offset, int to_r)
{
    InstrNode * new_instr = NULL;

    assert(Reg_IsRegister(to_r));

    switch(type) {
      case T_INT: case T_REF: case T_LONG: case T_IVOID:
        assert(Reg_GetType(to_r) == T_INT);
        new_instr = create_format5_instruction(LD, to_r, fp, offset, -1);
        CFG_InsertInstrAsSinglePred(cfg, instr, new_instr);
        break;
      case T_FLOAT:
        if (Reg_GetType(to_r) == T_FLOAT) {
            new_instr = create_format5_instruction(LDF, to_r, fp, offset, -1);
        } else {
            new_instr = create_format5_instruction(LD, to_r, fp, offset, -1);
        }
        CFG_InsertInstrAsSinglePred(cfg, instr, new_instr);
        break;
      case T_DOUBLE:
        if ((offset + 4) % 8 == 0) {
            new_instr = create_format5_instruction(LDDF, to_r, fp, 
                                                    offset - 4, -1);
        } else {
            assert(0 && "I doubt if this case occurs!\n");
            new_instr = create_format5_instruction(LDF, to_r, fp, 
                                                    offset + 4, -1);
            CFG_InsertInstrAsSinglePred(cfg, instr, new_instr);
            new_instr = create_format5_instruction(LDF, to_r + 1, fp, 
                                                    offset, -1);
        }
        CFG_InsertInstrAsSinglePred(cfg, instr, new_instr);
        break;
      case T_CC:
        assert(0);
        break;
      default:
        assert(0);
    }    

    if (Instr_IsVisited(instr)) Instr_MarkVisited(new_instr);
    return new_instr;
}

InstrNode *
RegAlloc_generate_promotion(InstrNode *instr, int type, int from_r, int to_r)
{
    return RegAlloc_generate_promotion_from_stack(instr, type,
				       RegAlloc_get_spill_offset(from_r), to_r);
}

InstrNode *
RegAlloc_promote_to_register(AllocStat *h, InstrNode *instr, int v, int to_r)
{

    int from_r = AllocStat_Find(h, v);

    assert(Var_IsVariable(v));
    assert(Reg_IsRegister(to_r));

    switch(Var_GetType(v)) {
      case T_INT: case T_REF: case T_LONG: case T_IVOID:
        assert(AllocStat_GetRefCount(h, to_r) == 0);

        AllocStat_ChangeReg(h, from_r, to_r);
        break;
      case T_FLOAT:
        assert(AllocStat_GetRefCount(h, to_r) == 0);        

        AllocStat_ChangeReg(h, from_r, to_r);
        break;
      case T_DOUBLE:
        assert(AllocStat_GetRefCount(h, to_r) == 0);
        assert(AllocStat_GetRefCount(h, to_r+1)==0);
        AllocStat_ChangeReg(h, from_r, to_r);
        break;
      case T_CC:
        assert(0);
        break;
      default:
        assert(0);
    }

    return
        RegAlloc_generate_promotion(instr, Var_GetType(v),
                          from_r, to_r);
}


// 
// Move the content at any storage to any storage.
// Can handle reg->reg, reg->mem, mem->reg, mem->mem
// 
void
RegAlloc_generate_general_copy(InstrNode * instr, int type, int from_r, int to_r)
{
    if (Reg_IsHW(from_r) && Reg_IsHW(to_r)) {
        // reg -> reg
        RegAlloc_generate_copy(instr, type, from_r, to_r);
    } else if (Reg_IsHW(from_r) && ! Reg_IsHW(to_r)) {        
        // reg -> mem
        RegAlloc_generate_spill(instr, type, from_r, to_r);
    } else if (! Reg_IsHW(from_r) && Reg_IsHW(to_r)) {
        // mem -> reg
        RegAlloc_generate_promotion(instr, type, from_r, to_r);
    } else {
        // mem -> mem
        RegAlloc_generate_promotion(instr, T_INT, from_r, g1);
        RegAlloc_generate_spill(instr, T_INT, g1, to_r);
        if (type == T_DOUBLE) {
            RegAlloc_generate_promotion(instr, T_INT, from_r + 1, g2);
            RegAlloc_generate_spill(instr, T_INT, g2, to_r + 1);
        }
    }
}

//
// RegAlloc_generate_copy_to_free_register()
//
void
RegAlloc_guarantee_register_free(AllocStat *h, InstrNode * instr, int from_r)
{
    if (AllocStat_GetRefCount(h, from_r)) {
        int v = AllocStat_FindRMap(h, from_r, 0);

// not confirmed bug --hacker
        if (v == -1) {
            from_r--;
            v = AllocStat_FindRMap(h, from_r, 0);
            assert(v != -1);
            assert(Var_GetType(v) == T_DOUBLE);
        }

        assert(v != -1);

        if (Reg_IsHW(from_r)) {
            int to_r = AllocStat_FindFreeReg(h, Var_GetType(v));

            if (Reg_IsHW(to_r)) {
                RegAlloc_copy_register(h, instr, from_r, to_r);
            } else {
                RegAlloc_spill_register(h, instr, from_r);
            }

        } else {
            int to_r = AllocStat_FindFreeSpill(h, Var_GetType(v));
            RegAlloc_generate_promotion(instr, T_INT, from_r, g1);
            RegAlloc_generate_spill(instr, T_INT, g1, to_r);
            if (Var_GetType(v) == T_DOUBLE) {
                RegAlloc_generate_promotion(instr, T_INT, from_r + 1, g1);
                RegAlloc_generate_spill(instr, T_INT, g1, to_r + 1);
            }

            AllocStat_ChangeReg(h, from_r, to_r);
        }
    }
}


//
// inserted by doner
//
void
RegAlloc_guarantee_double_registers_free(AllocStat *h, InstrNode * instr, int from_r)
{
    if (AllocStat_GetRefCount(h, from_r)) {
        int v = AllocStat_FindRMap(h, from_r, 0);

        assert(v != -1);

        if (Reg_IsHW(from_r)) {
            int to_r;

            // prevent the allocation of the next register
            h->refcount[from_r + 1]++;
            to_r = AllocStat_FindFreeReg(h, Var_GetType(v));
            h->refcount[from_r + 1]--;

            if (Reg_IsHW(to_r)) {
                RegAlloc_copy_register(h, instr, from_r, to_r);
            } else {
                RegAlloc_spill_register(h, instr, from_r);
            }

            if (AllocStat_GetRefCount(h, from_r + 1)) {
                int   v2 = AllocStat_FindRMap(h, from_r + 1, 0);

                assert(Reg_IsHW(from_r + 1));
                assert(v2 != -1);

                h->refcount[from_r]++;
                to_r = AllocStat_FindFreeReg(h, Var_GetType(v2));
                h->refcount[from_r]--;

                if (Reg_IsHW(to_r)) {
                    RegAlloc_copy_register(h, instr, from_r + 1, to_r);
                } else {
                    RegAlloc_spill_register(h, instr, from_r + 1);
                }
            }

        } else {
            int to_r;

            h->refcount[from_r + 1]++;
            to_r = AllocStat_FindFreeSpill(h, Var_GetType(v));
            h->refcount[from_r + 1]--;

            RegAlloc_generate_promotion(instr, T_INT, from_r, g1);
            RegAlloc_generate_spill(instr, T_INT, g1, to_r);
            if (Var_GetType(v) == T_DOUBLE) {
                RegAlloc_generate_promotion(instr, T_INT, from_r + 1, g1);
                RegAlloc_generate_spill(instr, T_INT, g1, to_r + 1);
            }

            AllocStat_ChangeReg(h, from_r, to_r);

            if (AllocStat_GetRefCount(h, from_r + 1)) {
                int   v2 = AllocStat_FindRMap(h, from_r + 1, 0);

                assert(!Reg_IsHW(from_r + 1));
                assert(v2 != -1);

                h->refcount[from_r]++;
                to_r = AllocStat_FindFreeSpill(h, Var_GetType(v2));
                h->refcount[from_r]--;

                RegAlloc_generate_promotion(instr, T_INT, from_r + 1, g1);
                RegAlloc_generate_spill(instr, T_INT, g1, to_r);
                if (Var_GetType(v2) == T_DOUBLE) {
                    RegAlloc_generate_promotion(instr, T_INT, from_r + 2, g1);
                    RegAlloc_generate_spill(instr, T_INT, g1, to_r + 1);
                }

                AllocStat_ChangeReg(h, from_r + 1, to_r);
            }
        }
    }

    if (AllocStat_GetRefCount(h, from_r + 1)) {
        h->refcount[from_r]++;
        RegAlloc_guarantee_register_free(h, instr, from_r + 1);
        h->refcount[from_r]--;
    }
}


int
RegAlloc_spill_non_src_register(AllocStat *h, InstrNode * instr, VarType type)
{
    int r = 0, k;

    switch(type) {
      case T_INT: case T_REF: case T_LONG: case T_IVOID:
        for(r = 1; r < 32; r++) {
            int used_as_source_register = 0;
            if (AllocStat_GetRefCount(h, r) > 0 &&
                 Reg_IsAvailable(r)) {
                for(k = 0; k < Instr_GetNumOfSrcRegs(instr); k++) {
                    int src_reg
                        = Instr_GetSrcReg(instr, k);
                    if (r == src_reg) {
                        used_as_source_register = 1;
                        break;
                    }
                }

                if (! used_as_source_register) {
                    RegAlloc_spill_register(h, instr, r);
                    break;
                }
            }
        }
        break;
      case T_FLOAT:
        for(r = f0; r <= f31; r++) {
            int used_as_source_register = 0;
            for(k = 0; k < Instr_GetNumOfSrcRegs(instr); k++) {
                int src_reg = Instr_GetSrcReg(instr, k);
                if (r == src_reg) {
                    used_as_source_register = 1;
                    break;
                }
            }
            if (! used_as_source_register) {
                RegAlloc_spill_register(h, instr, r);
                break;
            }
        }
        break;
      case T_DOUBLE:
        for(r = f0; r <= f31; r += 2) {
            int used_as_source_register = 0;
            for(k = 0; k < Instr_GetNumOfSrcRegs(instr); k++) {
                int src_reg = Instr_GetSrcReg(instr, k);
                if (r == src_reg || (r+1) == src_reg) {
                    used_as_source_register = 1;
                    break;
                }
            }
            if (! used_as_source_register) {
                // If "r" is a part of double, 
                // we don't have to spill r+1 because RegAlloc_spill_register()
                // spills "r+1" in such case.
                RegAlloc_spill_register(h, instr, r);

                // But if "r" is allocated "float",
                // we have to spill r+1. (And r+1 is always allocate "float".)
                if (AllocStat_GetRefCount(h, r+1) > 0) {
                    RegAlloc_spill_register(h, instr, r+1);
                }
                break;
            }
        }
        break;
      default:
        assert(0);
    }  // end switch              

    return r;
}


//
// modified by doner
//
void
RegAlloc_convert_long_to_double(AllocStat* h, int from_i_r, int to_f_r, InstrNode* succ)
{
    InstrNode*   new_instr;

    if (from_i_r % 2 == 0) {
        new_instr = create_format8_instruction(STD, from_i_r, fp, -8, -1);
        CFG_InsertInstrAsSinglePred(cfg, succ, new_instr);
        new_instr = create_format5_instruction(LDDF, to_f_r, fp, -8, -1);
        CFG_InsertInstrAsSinglePred(cfg, succ, new_instr);
    } else {
        new_instr = create_format8_instruction(ST, from_i_r, fp, -8, -1);
        CFG_InsertInstrAsSinglePred(cfg, succ, new_instr);
        new_instr = create_format8_instruction(ST, from_i_r + 1, fp, -4, -1);
        CFG_InsertInstrAsSinglePred(cfg, succ, new_instr);
        new_instr = create_format5_instruction(LDDF, to_f_r, fp, -8, -1);
        CFG_InsertInstrAsSinglePred(cfg, succ, new_instr);
    }
}

//
// modified by doner
//
void
RegAlloc_convert_int_to_float(AllocStat* h, int from_i_r, int to_f_r, InstrNode* succ)
{
    InstrNode*   new_instr;

    new_instr = create_format8_instruction(ST, from_i_r, fp, -8, 0);
    CFG_InsertInstrAsSinglePred(cfg, succ, new_instr);

    new_instr = create_format5_instruction(LDF, to_f_r, fp, -8, 0);
    CFG_InsertInstrAsSinglePred(cfg, succ, new_instr);
}

void
RegAlloc_load_double(int offset, int to_f_r, InstrNode* succ)
{
    InstrNode* new_instr;

    if (offset % 8 == 0) {
        /* double-word aligned - one LDDF is enough */
        new_instr = create_format5_instruction(LDDF, to_f_r, fp, offset, 0);
        CFG_InsertInstrAsSinglePred(cfg, succ, new_instr);
    } else {
        /* two LDFs are necessary */
        new_instr = create_format5_instruction(LDF, to_f_r, fp, offset, 0);
        CFG_InsertInstrAsSinglePred(cfg, succ, new_instr);

	/* VERY UGLY!!!!!!
	   We want to get a "FVOID" var whose number is one more than
	   that of 'to_f_r'. Need a more code cleaning someday!!!!
	 */
        new_instr = create_format5_instruction(LDF, ((to_f_r + 1) & (~TYPE_MASK)) | (T_FVOID << TYPE_SHIFT_SIZE), fp, offset+4, 0);
        CFG_InsertInstrAsSinglePred(cfg, succ, new_instr);
    }
}

void
RegAllloc_load_float(int offset, int to_f_r, InstrNode* succ)
{
    InstrNode* new_instr;

    new_instr = create_format5_instruction(LDF, to_f_r, fp, offset, 0);
    CFG_InsertInstrAsSinglePred(cfg, succ, new_instr);
}
