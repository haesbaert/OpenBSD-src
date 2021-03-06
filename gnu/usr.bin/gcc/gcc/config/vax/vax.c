/* Subroutines for insn-output.c for VAX.
   Copyright (C) 1987, 1994, 1995, 1997, 1998, 1999, 2000, 2001, 2002
   Free Software Foundation, Inc.

This file is part of GNU CC.

GNU CC is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2, or (at your option)
any later version.

GNU CC is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with GNU CC; see the file COPYING.  If not, write to
the Free Software Foundation, 59 Temple Place - Suite 330,
Boston, MA 02111-1307, USA.  */

#include "config.h"
#include "system.h"
#include "rtl.h"
#include "tree.h"
#include "regs.h"
#include "hard-reg-set.h"
#include "real.h"
#include "insn-config.h"
#include "conditions.h"
#include "function.h"
#include "output.h"
#include "insn-attr.h"
#include "recog.h"
#include "expr.h"
#include "flags.h"
#include "debug.h"
#include "tm_p.h"
#include "target.h"
#include "target-def.h"

static int follows_p PARAMS ((rtx, rtx));
static void vax_output_function_prologue PARAMS ((FILE *, HOST_WIDE_INT));
#if VMS_TARGET
static void vms_asm_out_constructor PARAMS ((rtx, int));
static void vms_asm_out_destructor PARAMS ((rtx, int));
static void vms_select_section PARAMS ((tree, int, unsigned HOST_WIDE_INT));
static void vms_encode_section_info PARAMS ((tree, int));
static void vms_globalize_label PARAMS ((FILE *, const char *));
#endif
static void vax_output_mi_thunk PARAMS ((FILE *, tree, HOST_WIDE_INT,
					 HOST_WIDE_INT, tree));

/* Initialize the GCC target structure.  */
#undef TARGET_ASM_ALIGNED_HI_OP
#define TARGET_ASM_ALIGNED_HI_OP "\t.word\t"

#undef TARGET_ASM_FUNCTION_PROLOGUE
#define TARGET_ASM_FUNCTION_PROLOGUE vax_output_function_prologue

#if VMS_TARGET
#undef TARGET_ASM_SELECT_SECTION
#define TARGET_ASM_SELECT_SECTION vms_select_section
#undef TARGET_ENCODE_SECTION_INFO
#define TARGET_ENCODE_SECTION_INFO vms_encode_section_info
#undef TARGET_ASM_GLOBALIZE_LABEL
#define TARGET_ASM_GLOBALIZE_LABEL vms_globalize_label
#endif

#undef TARGET_ASM_OUTPUT_MI_THUNK
#define TARGET_ASM_OUTPUT_MI_THUNK vax_output_mi_thunk
#undef TARGET_ASM_CAN_OUTPUT_MI_THUNK
#define TARGET_ASM_CAN_OUTPUT_MI_THUNK default_can_output_mi_thunk_no_vcall

struct gcc_target targetm = TARGET_INITIALIZER;

/* Set global variables as needed for the options enabled.  */

void
override_options ()
{
  /* We're VAX floating point, not IEEE floating point.  */
  memset (real_format_for_mode, 0, sizeof real_format_for_mode);
  real_format_for_mode[SFmode - QFmode] = &vax_f_format;
  real_format_for_mode[DFmode - QFmode]
    = (TARGET_G_FLOAT ? &vax_g_format : &vax_d_format);
}

/* Generate the assembly code for function entry.  FILE is a stdio
   stream to output the code to.  SIZE is an int: how many units of
   temporary storage to allocate.

   Refer to the array `regs_ever_live' to determine which registers to
   save; `regs_ever_live[I]' is nonzero if register number I is ever
   used in the function.  This function is responsible for knowing
   which registers should not be saved even if used.  */

static void
vax_output_function_prologue (file, size)
     FILE * file;
     HOST_WIDE_INT size;
{
  register int regno;
  register int mask = 0;

  for (regno = 0; regno < FIRST_PSEUDO_REGISTER; regno++)
    if (regs_ever_live[regno] && !call_used_regs[regno])
      mask |= 1 << regno;

  fprintf (file, "\t.word 0x%x\n", mask);

  if (dwarf2out_do_frame ())
    {
      const char *label = dwarf2out_cfi_label ();
      int offset = 0;

      for (regno = FIRST_PSEUDO_REGISTER-1; regno >= 0; --regno)
	if (regs_ever_live[regno] && !call_used_regs[regno])
	  dwarf2out_reg_save (label, regno, offset -= 4);

      dwarf2out_reg_save (label, PC_REGNUM, offset -= 4);
      dwarf2out_reg_save (label, FRAME_POINTER_REGNUM, offset -= 4);
      dwarf2out_reg_save (label, ARG_POINTER_REGNUM, offset -= 4);
      dwarf2out_def_cfa (label, FRAME_POINTER_REGNUM, -(offset - 4));
    }

  if (VMS_TARGET)
    {
      /* Adjusting the stack pointer by 4 before calling C$MAIN_ARGS
	 is required when linking with the VMS POSIX version of the C
	 run-time library; using `subl2 $4,r0' is adequate but we use
	 `clrl -(sp)' instead.  The extra 4 bytes could be removed
	 after the call because STARTING_FRAME_OFFSET's setting of -4
	 will end up adding them right back again, but don't bother.  */

      if (MAIN_NAME_P (DECL_NAME (current_function_decl)))
	asm_fprintf (file, "\tclrl -(%Rsp)\n\tjsb _C$MAIN_ARGS\n");
    }

  size -= STARTING_FRAME_OFFSET;

  if (warn_stack_larger_than && size > stack_larger_than_size)
    warning ("stack usage is %d bytes", size);

  if (size >= 64)
    asm_fprintf (file, "\tmovab %d(%Rsp),%Rsp\n", -size);
  else if (size)
    asm_fprintf (file, "\tsubl2 $%d,%Rsp\n", size);
}

/* This is like nonimmediate_operand with a restriction on the type of MEM.  */

void
split_quadword_operands (operands, low, n)
     rtx *operands, *low;
     int n ATTRIBUTE_UNUSED;
{
  int i;
  /* Split operands.  */

  low[0] = low[1] = low[2] = 0;
  for (i = 0; i < 3; i++)
    {
      if (low[i])
	/* it's already been figured out */;
      else if (GET_CODE (operands[i]) == MEM
	       && (GET_CODE (XEXP (operands[i], 0)) == POST_INC))
	{
	  rtx addr = XEXP (operands[i], 0);
	  operands[i] = low[i] = gen_rtx_MEM (SImode, addr);
	  if (which_alternative == 0 && i == 0)
	    {
	      addr = XEXP (operands[i], 0);
	      operands[i+1] = low[i+1] = gen_rtx_MEM (SImode, addr);
	    }
	}
      else
	{
	  low[i] = operand_subword (operands[i], 0, 0, DImode);
	  operands[i] = operand_subword (operands[i], 1, 0, DImode);
	}
    }
}

void
print_operand_address (file, addr)
     FILE *file;
     register rtx addr;
{
  register rtx reg1, breg, ireg;
  rtx offset;

 retry:
  switch (GET_CODE (addr))
    {
    case MEM:
      fprintf (file, "*");
      addr = XEXP (addr, 0);
      goto retry;

    case REG:
      fprintf (file, "(%s)", reg_names[REGNO (addr)]);
      break;

    case PRE_DEC:
      fprintf (file, "-(%s)", reg_names[REGNO (XEXP (addr, 0))]);
      break;

    case POST_INC:
      fprintf (file, "(%s)+", reg_names[REGNO (XEXP (addr, 0))]);
      break;

    case PLUS:
      /* There can be either two or three things added here.  One must be a
	 REG.  One can be either a REG or a MULT of a REG and an appropriate
	 constant, and the third can only be a constant or a MEM.

	 We get these two or three things and put the constant or MEM in
	 OFFSET, the MULT or REG in IREG, and the REG in BREG.  If we have
	 a register and can't tell yet if it is a base or index register,
	 put it into REG1.  */

      reg1 = 0; ireg = 0; breg = 0; offset = 0;

      if (CONSTANT_ADDRESS_P (XEXP (addr, 0))
	  || GET_CODE (XEXP (addr, 0)) == MEM)
	{
	  offset = XEXP (addr, 0);
	  addr = XEXP (addr, 1);
	}
      else if (CONSTANT_ADDRESS_P (XEXP (addr, 1))
	       || GET_CODE (XEXP (addr, 1)) == MEM)
	{
	  offset = XEXP (addr, 1);
	  addr = XEXP (addr, 0);
	}
      else if (GET_CODE (XEXP (addr, 1)) == MULT)
	{
	  ireg = XEXP (addr, 1);
	  addr = XEXP (addr, 0);
	}
      else if (GET_CODE (XEXP (addr, 0)) == MULT)
	{
	  ireg = XEXP (addr, 0);
	  addr = XEXP (addr, 1);
	}
      else if (GET_CODE (XEXP (addr, 1)) == REG)
	{
	  reg1 = XEXP (addr, 1);
	  addr = XEXP (addr, 0);
	}
      else if (GET_CODE (XEXP (addr, 0)) == REG)
	{
	  reg1 = XEXP (addr, 0);
	  addr = XEXP (addr, 1);
	}
      else
	abort ();

      if (GET_CODE (addr) == REG)
	{
	  if (reg1)
	    ireg = addr;
	  else
	    reg1 = addr;
	}
      else if (GET_CODE (addr) == MULT)
	ireg = addr;
      else if (GET_CODE (addr) == PLUS)
	{
	  if (CONSTANT_ADDRESS_P (XEXP (addr, 0))
	      || GET_CODE (XEXP (addr, 0)) == MEM)
	    {
	      if (offset)
		{
		  if (GET_CODE (offset) == CONST_INT)
		    offset = plus_constant (XEXP (addr, 0), INTVAL (offset));
		  else if (GET_CODE (XEXP (addr, 0)) == CONST_INT)
		    offset = plus_constant (offset, INTVAL (XEXP (addr, 0)));
		  else
		    abort ();
		}
	      offset = XEXP (addr, 0);
	    }
	  else if (GET_CODE (XEXP (addr, 0)) == REG)
	    {
	      if (reg1)
		ireg = reg1, breg = XEXP (addr, 0), reg1 = 0;
	      else
		reg1 = XEXP (addr, 0);
	    }
	  else if (GET_CODE (XEXP (addr, 0)) == MULT)
	    {
	      if (ireg)
		abort ();
	      ireg = XEXP (addr, 0);
	    }
	  else
	    abort ();

	  if (CONSTANT_ADDRESS_P (XEXP (addr, 1))
	      || GET_CODE (XEXP (addr, 1)) == MEM)
	    {
	      if (offset)
		{
		  if (GET_CODE (offset) == CONST_INT)
		    offset = plus_constant (XEXP (addr, 1), INTVAL (offset));
		  else if (GET_CODE (XEXP (addr, 1)) == CONST_INT)
		    offset = plus_constant (offset, INTVAL (XEXP (addr, 1)));
		  else
		    abort ();
		}
	      offset = XEXP (addr, 1);
	    }
	  else if (GET_CODE (XEXP (addr, 1)) == REG)
	    {
	      if (reg1)
		ireg = reg1, breg = XEXP (addr, 1), reg1 = 0;
	      else
		reg1 = XEXP (addr, 1);
	    }
	  else if (GET_CODE (XEXP (addr, 1)) == MULT)
	    {
	      if (ireg)
		abort ();
	      ireg = XEXP (addr, 1);
	    }
	  else
	    abort ();
	}
      else
	abort ();

      /* If REG1 is nonzero, figure out if it is a base or index register.  */
      if (reg1)
	{
	  if (breg != 0 || (offset && GET_CODE (offset) == MEM))
	    {
	      if (ireg)
		abort ();
	      ireg = reg1;
	    }
	  else
	    breg = reg1;
	}

      if (offset != 0)
	output_address (offset);

      if (breg != 0)
	fprintf (file, "(%s)", reg_names[REGNO (breg)]);

      if (ireg != 0)
	{
	  if (GET_CODE (ireg) == MULT)
	    ireg = XEXP (ireg, 0);
	  if (GET_CODE (ireg) != REG)
	    abort ();
	  fprintf (file, "[%s]", reg_names[REGNO (ireg)]);
	}
      break;

    default:
      output_addr_const (file, addr);
    }
}

const char *
rev_cond_name (op)
     rtx op;
{
  switch (GET_CODE (op))
    {
    case EQ:
      return "neq";
    case NE:
      return "eql";
    case LT:
      return "geq";
    case LE:
      return "gtr";
    case GT:
      return "leq";
    case GE:
      return "lss";
    case LTU:
      return "gequ";
    case LEU:
      return "gtru";
    case GTU:
      return "lequ";
    case GEU:
      return "lssu";

    default:
      abort ();
    }
}

int
vax_float_literal(c)
    register rtx c;
{
  register enum machine_mode mode;
  REAL_VALUE_TYPE r, s;
  int i;

  if (GET_CODE (c) != CONST_DOUBLE)
    return 0;

  mode = GET_MODE (c);

  if (c == const_tiny_rtx[(int) mode][0]
      || c == const_tiny_rtx[(int) mode][1]
      || c == const_tiny_rtx[(int) mode][2])
    return 1;

  REAL_VALUE_FROM_CONST_DOUBLE (r, c);

  for (i = 0; i < 7; i++)
    {
      int x = 1 << i;
      REAL_VALUE_FROM_INT (s, x, 0, mode);

      if (REAL_VALUES_EQUAL (r, s))
	return 1;
      if (!exact_real_inverse (mode, &s))
	abort ();
      if (REAL_VALUES_EQUAL (r, s))
	return 1;
    }
  return 0;
}


/* Return the cost in cycles of a memory address, relative to register
   indirect.

   Each of the following adds the indicated number of cycles:

   1 - symbolic address
   1 - pre-decrement
   1 - indexing and/or offset(register)
   2 - indirect */


int
vax_address_cost (addr)
    register rtx addr;
{
  int reg = 0, indexed = 0, indir = 0, offset = 0, predec = 0;
  rtx plus_op0 = 0, plus_op1 = 0;
 restart:
  switch (GET_CODE (addr))
    {
    case PRE_DEC:
      predec = 1;
    case REG:
    case SUBREG:
    case POST_INC:
      reg = 1;
      break;
    case MULT:
      indexed = 1;	/* 2 on VAX 2 */
      break;
    case CONST_INT:
      /* byte offsets cost nothing (on a VAX 2, they cost 1 cycle) */
      if (offset == 0)
	offset = (unsigned HOST_WIDE_INT)(INTVAL(addr)+128) > 256;
      break;
    case CONST:
    case SYMBOL_REF:
      offset = 1;	/* 2 on VAX 2 */
      break;
    case LABEL_REF:	/* this is probably a byte offset from the pc */
      if (offset == 0)
	offset = 1;
      break;
    case PLUS:
      if (plus_op0)
	plus_op1 = XEXP (addr, 0);
      else
	plus_op0 = XEXP (addr, 0);
      addr = XEXP (addr, 1);
      goto restart;
    case MEM:
      indir = 2;	/* 3 on VAX 2 */
      addr = XEXP (addr, 0);
      goto restart;
    default:
      break;
    }

  /* Up to 3 things can be added in an address.  They are stored in
     plus_op0, plus_op1, and addr.  */

  if (plus_op0)
    {
      addr = plus_op0;
      plus_op0 = 0;
      goto restart;
    }
  if (plus_op1)
    {
      addr = plus_op1;
      plus_op1 = 0;
      goto restart;
    }
  /* Indexing and register+offset can both be used (except on a VAX 2)
     without increasing execution time over either one alone.  */
  if (reg && indexed && offset)
    return reg + indir + offset + predec;
  return reg + indexed + indir + offset + predec;
}


/* Cost of an expression on a VAX.  This version has costs tuned for the
   CVAX chip (found in the VAX 3 series) with comments for variations on
   other models.  */

int
vax_rtx_cost (x)
    register rtx x;
{
  register enum rtx_code code = GET_CODE (x);
  enum machine_mode mode = GET_MODE (x);
  register int c;
  int i = 0;				/* may be modified in switch */
  const char *fmt = GET_RTX_FORMAT (code); /* may be modified in switch */

  switch (code)
    {
    case POST_INC:
      return 2;
    case PRE_DEC:
      return 3;
    case MULT:
      switch (mode)
	{
	case DFmode:
	  c = 16;		/* 4 on VAX 9000 */
	  break;
	case SFmode:
	  c = 9;		/* 4 on VAX 9000, 12 on VAX 2 */
	  break;
	case DImode:
	  c = 16;		/* 6 on VAX 9000, 28 on VAX 2 */
	  break;
	case SImode:
	case HImode:
	case QImode:
	  c = 10;		/* 3-4 on VAX 9000, 20-28 on VAX 2 */
	  break;
	default:
	  return MAX_COST;	/* Mode is not supported.  */
	}
      break;
    case UDIV:
      if (mode != SImode)
	return MAX_COST;	/* Mode is not supported.  */
      c = 17;
      break;
    case DIV:
      if (mode == DImode)
	c = 30;	/* highly variable */
      else if (mode == DFmode)
	/* divide takes 28 cycles if the result is not zero, 13 otherwise */
	c = 24;
      else
	c = 11;			/* 25 on VAX 2 */
      break;
    case MOD:
      c = 23;
      break;
    case UMOD:
      if (mode != SImode)
	return MAX_COST;	/* Mode is not supported.  */
      c = 29;
      break;
    case FLOAT:
      c = 6 + (mode == DFmode) + (GET_MODE (XEXP (x, 0)) != SImode);
      /* 4 on VAX 9000 */
      break;
    case FIX:
      c = 7;			/* 17 on VAX 2 */
      break;
    case ASHIFT:
    case LSHIFTRT:
    case ASHIFTRT:
      if (mode == DImode)
	c = 12;
      else
	c = 10;			/* 6 on VAX 9000 */
      break;
    case ROTATE:
    case ROTATERT:
      c = 6;			/* 5 on VAX 2, 4 on VAX 9000 */
      if (GET_CODE (XEXP (x, 1)) == CONST_INT)
	fmt = "e";	/* all constant rotate counts are short */
      break;
    case PLUS:
    case MINUS:
      c = (mode == DFmode) ? 13 : 8;	/* 6/8 on VAX 9000, 16/15 on VAX 2 */
      /* Small integer operands can use subl2 and addl2.  */
      if ((GET_CODE (XEXP (x, 1)) == CONST_INT)
	  && (unsigned HOST_WIDE_INT)(INTVAL (XEXP (x, 1)) + 63) < 127)
	fmt = "e";
      break;
    case IOR:
    case XOR:
      c = 3;
      break;
    case AND:
      /* AND is special because the first operand is complemented.  */
      c = 3;
      if (GET_CODE (XEXP (x, 0)) == CONST_INT)
	{
	  if ((unsigned HOST_WIDE_INT)~INTVAL (XEXP (x, 0)) > 63)
	    c = 4;
	  fmt = "e";
	  i = 1;
	}
      break;
    case NEG:
      if (mode == DFmode)
	return 9;
      else if (mode == SFmode)
	return 6;
      else if (mode == DImode)
	return 4;
    case NOT:
      return 2;
    case ZERO_EXTRACT:
    case SIGN_EXTRACT:
      c = 15;
      break;
    case MEM:
      if (mode == DImode || mode == DFmode)
	c = 5;				/* 7 on VAX 2 */
      else
	c = 3;				/* 4 on VAX 2 */
      x = XEXP (x, 0);
      if (GET_CODE (x) == REG || GET_CODE (x) == POST_INC)
	return c;
      return c + vax_address_cost (x);
    default:
      c = 3;
      break;
    }


  /* Now look inside the expression.  Operands which are not registers or
     short constants add to the cost.

     FMT and I may have been adjusted in the switch above for instructions
     which require special handling */

  while (*fmt++ == 'e')
    {
      register rtx op = XEXP (x, i++);
      code = GET_CODE (op);

      /* A NOT is likely to be found as the first operand of an AND
	 (in which case the relevant cost is of the operand inside
	 the not) and not likely to be found anywhere else.  */
      if (code == NOT)
	op = XEXP (op, 0), code = GET_CODE (op);

      switch (code)
	{
	case CONST_INT:
	  if ((unsigned HOST_WIDE_INT)INTVAL (op) > 63
	      && GET_MODE (x) != QImode)
	    c += 1;		/* 2 on VAX 2 */
	  break;
	case CONST:
	case LABEL_REF:
	case SYMBOL_REF:
	  c += 1;		/* 2 on VAX 2 */
	  break;
	case CONST_DOUBLE:
	  if (GET_MODE_CLASS (GET_MODE (op)) == MODE_FLOAT)
	    {
	      /* Registers are faster than floating point constants -- even
		 those constants which can be encoded in a single byte.  */
	      if (vax_float_literal (op))
		c++;
	      else
		c += (GET_MODE (x) == DFmode) ? 3 : 2;
	    }
	  else
	    {
	      if (CONST_DOUBLE_HIGH (op) != 0
		  || (unsigned)CONST_DOUBLE_LOW (op) > 63)
		c += 2;
	    }
	  break;
	case MEM:
	  c += 1;		/* 2 on VAX 2 */
	  if (GET_CODE (XEXP (op, 0)) != REG)
	    c += vax_address_cost (XEXP (op, 0));
	  break;
	case REG:
	case SUBREG:
	  break;
	default:
	  c += 1;
	  break;
	}
    }
  return c;
}

#if VMS_TARGET
/* Additional support code for VMS target.  */

/* Linked list of all externals that are to be emitted when optimizing
   for the global pointer if they haven't been declared by the end of
   the program with an appropriate .comm or initialization.  */

static
struct extern_list {
  struct extern_list *next;	/* next external */
  const char *name;		/* name of the external */
  int size;			/* external's actual size */
  int in_const;			/* section type flag */
} *extern_head = 0, *pending_head = 0;

/* Check whether NAME is already on the external definition list.  If not,
   add it to either that list or the pending definition list.  */

void
vms_check_external (decl, name, pending)
     tree decl;
     const char *name;
     int pending;
{
  register struct extern_list *p, *p0;

  for (p = extern_head; p; p = p->next)
    if (!strcmp (p->name, name))
      return;

  for (p = pending_head, p0 = 0; p; p0 = p, p = p->next)
    if (!strcmp (p->name, name))
      {
	if (pending)
	  return;

	/* Was pending, but has now been defined; move it to other list.  */
	if (p == pending_head)
	  pending_head = p->next;
	else
	  p0->next = p->next;
	p->next = extern_head;
	extern_head = p;
	return;
      }

  /* Not previously seen; create a new list entry.  */
  p = (struct extern_list *) xmalloc (sizeof (struct extern_list));
  p->name = name;

  if (pending)
    {
      /* Save the size and section type and link to `pending' list.  */
      p->size = (DECL_SIZE (decl) == 0) ? 0 :
	TREE_INT_CST_LOW (size_binop (CEIL_DIV_EXPR, DECL_SIZE (decl),
				      size_int (BITS_PER_UNIT)));
      p->in_const = (TREE_READONLY (decl) && ! TREE_THIS_VOLATILE (decl));

      p->next = pending_head;
      pending_head = p;
    }
  else
    {
      /* Size and section type don't matter; link to `declared' list.  */
      p->size = p->in_const = 0;        /* arbitrary init */

      p->next = extern_head;
      extern_head = p;
    }
  return;
}

void
vms_flush_pending_externals (file)
     FILE *file;
{
  register struct extern_list *p;

  while (pending_head)
    {
      /* Move next pending declaration to the "done" list.  */
      p = pending_head;
      pending_head = p->next;
      p->next = extern_head;
      extern_head = p;

      /* Now output the actual declaration.  */
      if (p->in_const)
	const_section ();
      else
	data_section ();
      fputs (".comm ", file);
      assemble_name (file, p->name);
      fprintf (file, ",%d\n", p->size);
    }
}

static void
vms_asm_out_constructor (symbol, priority)
     rtx symbol;
     int priority ATTRIBUTE_UNUSED;
{
  fprintf (asm_out_file,".globl $$PsectAttributes_NOOVR$$__gxx_init_1\n");
  data_section();
  fprintf (asm_out_file,"$$PsectAttributes_NOOVR$$__gxx_init_1:\n\t.long\t");
  assemble_name (asm_out_file, XSTR (symbol, 0));
  fputc ('\n', asm_out_file);
}

static void
vms_asm_out_destructor (symbol, priority)
     rtx symbol;
     int priority ATTRIBUTE_UNUSED;
{
  fprintf (asm_out_file,".globl $$PsectAttributes_NOOVR$$__gxx_clean_1\n");
  data_section();
  fprintf (asm_out_file,"$$PsectAttributes_NOOVR$$__gxx_clean_1:\n\t.long\t");
  assemble_name (asm_out_file, XSTR (symbol, 0));
  fputc ('\n', asm_out_file);
}

static void
vms_select_section (exp, reloc, align)
     tree exp;
     int reloc ATTRIBUTE_UNUSED;
     unsigned HOST_WIDE_INT align ATTRIBUTE_UNUSED;
{
  if (TREE_CODE (exp) == VAR_DECL)
    {
      if (TREE_READONLY (exp) && ! TREE_THIS_VOLATILE (exp)
	  && DECL_INITIAL (exp)
	  && (DECL_INITIAL (exp) == error_mark_node
	      || TREE_CONSTANT (DECL_INITIAL (exp))))
	{
	  if (TREE_PUBLIC (exp))
	    const_section ();
	  else
	    text_section ();
	}
      else
	data_section ();
    }
  if (TREE_CODE_CLASS (TREE_CODE (exp)) == 'c')
    {
      if (TREE_CODE (exp) == STRING_CST && flag_writable_strings)
	data_section ();
      else
	text_section ();
    }
}

/* Make sure that external variables are correctly addressed.  Under VMS
   there is some brain damage in the linker that requires us to do this.  */

static void
vms_encode_section_info (decl, first)
     tree decl;
     int first ATTRIBUTE_UNUSED;
{
  if (DECL_EXTERNAL (decl) && TREE_PUBLIC (decl))
    SYMBOL_REF_FLAG (XEXP (DECL_RTL (decl), 0)) = 1;
}

/* This is how to output a command to make the user-level label named NAME
   defined for reference from other files.  */
static void
vms_globalize_label (stream, name)
     FILE *stream;
     const char *name;
{
  default_globalize_label (stream, name);
  vms_check_external (NULL_TREE, name, 0);
}
#endif /* VMS_TARGET */

/* Additional support code for VMS host.  */
/* ??? This should really be in libiberty; vax.c is a target file.  */
#ifdef QSORT_WORKAROUND
  /*
	Do not use VAXCRTL's qsort() due to a severe bug:  once you've
	sorted something which has a size that's an exact multiple of 4
	and is longword aligned, you cannot safely sort anything which
	is either not a multiple of 4 in size or not longword aligned.
	A static "move-by-longword" optimization flag inside qsort() is
	never reset.  This is known to affect VMS V4.6 through VMS V5.5-1,
	and was finally fixed in VMS V5.5-2.

	In this work-around an insertion sort is used for simplicity.
	The qsort code from glibc should probably be used instead.
   */
void
not_qsort (array, count, size, compare)
     void *array;
     unsigned count, size;
     int (*compare)();
{

  if (size == sizeof (short))
    {
      register int i;
      register short *next, *prev;
      short tmp, *base = array;

      for (next = base, i = count - 1; i > 0; i--)
	{
	  prev = next++;
	  if ((*compare)(next, prev) < 0)
	    {
	      tmp = *next;
	      do  *(prev + 1) = *prev;
		while (--prev >= base ? (*compare)(&tmp, prev) < 0 : 0);
	      *(prev + 1) = tmp;
	    }
	}
    }
  else if (size == sizeof (long))
    {
      register int i;
      register long *next, *prev;
      long tmp, *base = array;

      for (next = base, i = count - 1; i > 0; i--)
	{
	  prev = next++;
	  if ((*compare)(next, prev) < 0)
	    {
	      tmp = *next;
	      do  *(prev + 1) = *prev;
		while (--prev >= base ? (*compare)(&tmp, prev) < 0 : 0);
	      *(prev + 1) = tmp;
	    }
	}
    }
  else  /* arbitrary size */
    {
      register int i;
      register char *next, *prev, *tmp = alloca (size), *base = array;

      for (next = base, i = count - 1; i > 0; i--)
	{   /* count-1 forward iterations */
	  prev = next,  next += size;		/* increment front pointer */
	  if ((*compare)(next, prev) < 0)
	    {	/* found element out of order; move others up then re-insert */
	      memcpy (tmp, next, size);		/* save smaller element */
	      do { memcpy (prev + size, prev, size); /* move larger elem. up */
		   prev -= size;		/* decrement back pointer */
		 } while (prev >= base ? (*compare)(tmp, prev) < 0 : 0);
	      memcpy (prev + size, tmp, size);	/* restore small element */
	    }
	}
#ifdef USE_C_ALLOCA
      alloca (0);
#endif
    }

  return;
}
#endif /* QSORT_WORKAROUND */

/* Return 1 if insn A follows B.  */

static int
follows_p (a, b)
     rtx a, b;
{
  register rtx p;

  for (p = a; p != b; p = NEXT_INSN (p))
    if (! p)
      return 1;

  return 0;
}

/* Returns 1 if we know operand OP was 0 before INSN.  */

int
reg_was_0_p (insn, op)
     rtx insn, op;
{
  rtx link;

  return ((link = find_reg_note (insn, REG_WAS_0, 0))
	  /* Make sure the insn that stored the 0 is still present
	     and doesn't follow INSN in the insn sequence.  */
	  && ! INSN_DELETED_P (XEXP (link, 0))
	  && GET_CODE (XEXP (link, 0)) != NOTE
	  && ! follows_p (XEXP (link, 0), insn)
	  /* Make sure cross jumping didn't happen here.  */
	  && no_labels_between_p (XEXP (link, 0), insn)
	  /* Make sure the reg hasn't been clobbered.  */
	  && ! reg_set_between_p (op, XEXP (link, 0), insn));
}

static void
vax_output_mi_thunk (file, thunk, delta, vcall_offset, function)
     FILE *file;
     tree thunk ATTRIBUTE_UNUSED;
     HOST_WIDE_INT delta;
     HOST_WIDE_INT vcall_offset ATTRIBUTE_UNUSED;
     tree function;
{
  fprintf (file, "\t.word 0x0ffc\n");					
  fprintf (file, "\taddl2 $");
  fprintf (file, HOST_WIDE_INT_PRINT_DEC, delta);
  asm_fprintf (file, ",4(%Rap)\n");
  fprintf (file, "\tjmp ");						
  assemble_name (file,  XSTR (XEXP (DECL_RTL (function), 0), 0));	
  fprintf (file, "+2\n");						
}
