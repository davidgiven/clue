/* cg-lisp.c
 * Backend for Common Lisp
 *
 * © 2008 Peter Maydell.
 * Clue is licensed under the Revised BSD open source license. To get the
 * full license text, see the README file.
 *
 * $Id$
 * $HeadURL$
 * $LastChangedDate: 2007-04-30 22:41:42 +0000 (Mon, 30 Apr 2007) $
 */

#include "globals.h"

static int function_parens = 0;
static int function_arg_list = 0;
static int function_is_initializer = 0;
static int register_count;
static int parencount = 0;

/* Reset the register tracking. */

static void cg_reset_registers(void)
{
	register_count = 0;
}

/* Initialize a new hardreg. */

static void cg_init_register(struct hardreg* reg, int regclass)
{
	assert(!reg->name);
	reg->name = aprintf("H%d", register_count);
	register_count++;
}

/* Get the name of a register. */

static const char* cg_get_register_name(struct hardreg* reg)
{
	assert(reg->name);
	return reg->name;
}

/* Emit the file prologue. */

static void cg_prologue(void)
{
	zprintf(";;; cg_prologue\n");
}

/* Emit the file epilogue. */

static void cg_epilogue(void)
{
	zprintf(";;; cg_epilogue\n");
}

/* Emit a comment (contains no actual code). */

static void cg_comment(const char* format, ...)
{
	if (verbose)
	{
		va_list ap;
		va_start(ap, format);
		zprintf("; ");
		zvprintf(format, ap);
		va_end(ap);
	}
}

static void cg_declare_slot(struct symbol* sym, unsigned size)
{
	zprintf("(defvar %s)\n", show_symbol_mangled(sym));
}

static void cg_declare_function(struct symbol* sym, int returning)
{
	cg_declare_slot(sym, 0);
}

static void cg_declare_function_arg(int regclass)
{
}

static void cg_declare_function_vararg(void)
{
}

static void cg_declare_function_end(void)
{
}

static void cg_create_storage(struct symbol* sym, unsigned size)
{
	zprintf("(setf %s (make-array 0 :fill-pointer 0 :adjustable t))\n", show_symbol_mangled(sym));
}

static void cg_import(struct symbol* sym)
{
	zprintf(";;; cg_import %s\n", show_symbol_mangled(sym));
}

static void cg_export(struct symbol* sym)
{
	zprintf(";;; cg_export %s\n", show_symbol_mangled(sym));
}

static void cg_function_prologue(struct symbol* sym, int returning)
{
	if (!sym)
	{
		zprintf("(clue-add-initializer (lambda (");
		function_is_initializer = 1;
	}
	else
	{
		zprintf("(setf %s (lambda (", show_symbol_mangled(sym));
		function_is_initializer = 0;
	}
	parencount = 2; /* we don't count the arglist's bracket */
	function_arg_list = 0;
}

static void cg_function_prologue_arg(struct hardreg* reg)
{
	if (function_arg_list > 0)
		zprintf(" ");

	zprintf("%s", show_hardreg(reg));
	function_arg_list++;
}

static void cg_function_prologue_vararg(void)
{
}

static void cg_function_prologue_reg(struct hardreg* reg)
{
	if (function_arg_list >= 0)
	{
		/* First reg. Terminate arg list and open the prog.
		 * prog gives us a tagbody we can use for go tags, and a block
		 * we can use with 'return', and some space to declare locals.
		 * It is the program feature!
		 */
		zprintf(")\n(prog (");
		function_arg_list = -1;
		parencount++;
	}
	
	zprintf("%s ", show_hardreg(reg));
}

static void cg_function_prologue_end(void)
{
	/* Close the prog's list of variables */
	zprintf(")\n");
}

static void cg_function_epilogue(void)
{
	if (function_is_initializer)
		zprintf(";;; cg_function_epilogue for initializer \n\n");

	while (parencount)
	{
		parencount--;
		zprintf(")");
	}
	zprintf("\n\n");
}

/* Starts a basic block. */

static void cg_bb_start(struct binfo* binfo)
{
	if (binfo->id != 0)
		zprintf("L_%d\n", binfo->id);
}

/* Ends a basic block in an unconditional jump. */

static void cg_bb_end_jump(struct binfo* target)
{
	zprintf("(go L_%d)\n", target->id);
}

/* Ends a basic block in a conditional jump based on any value. */

static void cg_bb_end_if(struct hardreg* cond,
		struct binfo* truetarget, struct binfo* falsetarget)
{
	zprintf("(if %s (go L_%d) (go L_%d))\n",
			show_hardreg(cond), truetarget->id, falsetarget->id);
}

/* Copies a single register. */

static void cg_copy(struct hardreg* src, struct hardreg* dest)
{
	if (src != dest)
		zprintf("(setf %s %s)\n", show_hardreg(dest), show_hardreg(src));
}

/* Loads a value from a memory location. */

static void cg_load(struct hardreg* simple, struct hardreg* base,
		int offset, struct hardreg* dest)
{
	zprintf("(setf %s (aref %s (+ %s %d)))\n",
			show_hardreg(dest),
			show_hardreg(base),
			show_hardreg(simple),
			offset);
}

/* Stores a value from a memory location. */

static void cg_store(struct hardreg* simple, struct hardreg* base,
		int offset, struct hardreg* src)
{
	/* Use a helper function because aref doesn't autoextend the array */
	if (simple)
		zprintf("(clue-set-aref %s (+ %s %d) %s)\n", show_hardreg(base),
			show_hardreg(simple), offset, show_hardreg(src));
	else
		zprintf("(clue-set-aref %s %d %s)\n", show_hardreg(base), offset, show_hardreg(src));
}

/* Load a constant int. */

static void cg_set_int(long long int value, struct hardreg* dest)
{
	zprintf("(setf %s %lld)\n", show_hardreg(dest), value);
}

/* Load a constant float. */

static void cg_set_float(long double value, struct hardreg* dest)
{
	zprintf("(setf %s %llf)\n", show_hardreg(dest), value);
}

/* Load a constant symbol. */

static void cg_set_symbol(struct symbol* sym, struct hardreg* dest)
{
	zprintf("(setf %s %s)\n", show_hardreg(dest),
			sym ? show_symbol_mangled(sym) : "nil");
}

/* Convert to integer. */

static void cg_toint(struct hardreg* src, struct hardreg* dest)
{
	zprintf("(setf %s (truncate %s))\n", show_hardreg(dest), show_hardreg(src));
}

/* Arithmetic negation. */

static void cg_negate(struct hardreg* src, struct hardreg* dest)
{
	zprintf("(setf %s (- 0 %s))\n", show_hardreg(dest), show_hardreg(src));
}

#define SIMPLE_PREFIX_2OP(NAME, OP) \
	static void cg_##NAME(struct hardreg* src1, struct hardreg* src2, \
			struct hardreg* dest) \
	{ \
		zprintf("(setf %s (" OP " %s %s))\n", show_hardreg(dest), \
				show_hardreg(src1), show_hardreg(src2)); \
	}

SIMPLE_PREFIX_2OP(add, "+")
SIMPLE_PREFIX_2OP(subtract, "-")
SIMPLE_PREFIX_2OP(multiply, "*")
SIMPLE_PREFIX_2OP(divide, "/")
SIMPLE_PREFIX_2OP(mod, "mod")
SIMPLE_PREFIX_2OP(logand, "logand")
SIMPLE_PREFIX_2OP(logor, "logior")
SIMPLE_PREFIX_2OP(logxor, "logxor")
SIMPLE_PREFIX_2OP(shl, "ash")

static void cg_shr(struct hardreg* src1, struct hardreg* src2, struct hardreg* dest)
{
	/* CL doesn't have a separate shift right, you use ash with a negative shift count */
	zprintf("(setf %s (ash (- 0 %s) %s))\n", show_hardreg(dest),
		show_hardreg(src1), show_hardreg(src2));
}

#define SIMPLE_SET_2OP(NAME, OP) \
	static void cg_##NAME(struct hardreg* src1, struct hardreg* src2, \
			struct hardreg* dest) \
	{ \
		zprintf("(setf %s (if (" OP " %s %s) 1 0))\n", show_hardreg(dest), \
				show_hardreg(src1), show_hardreg(src2)); \
	}

SIMPLE_SET_2OP(booland, "and")
SIMPLE_SET_2OP(boolor, "or")
SIMPLE_SET_2OP(set_gt, ">")
SIMPLE_SET_2OP(set_ge, ">=")
SIMPLE_SET_2OP(set_lt, "<")
SIMPLE_SET_2OP(set_le, "<=")
SIMPLE_SET_2OP(set_eq, "==")
SIMPLE_SET_2OP(set_ne, "/=")

/* Select operations using any condition. */

static void cg_select(struct hardreg* cond,
		struct hardreg* dest1, struct hardreg* dest2,
		struct hardreg* true1, struct hardreg* true2,
		struct hardreg* false1, struct hardreg* false2)
{
	zprintf("(if %s\n", show_hardreg(cond));
	if (dest2)
		zprintf("(progn (setf %s %s) (setf %s %s))\n(progn (setf %s %s) (setf %s %s)))\n",
				show_hardreg(dest1), show_hardreg(true1),
				show_hardreg(dest2), show_hardreg(true2),
				show_hardreg(dest1), show_hardreg(false1),
				show_hardreg(dest2), show_hardreg(false2));
	else
		zprintf("(setf %s %s) (setf %s %s))\n",
				show_hardreg(dest1), show_hardreg(true1),
				show_hardreg(dest1), show_hardreg(false1));
}

static void cg_call(struct hardreg* func,
		struct hardreg* dest1, struct hardreg* dest2)
{
	function_parens = 1;
	if (dest1)
	{
		function_parens++;
		if (dest2)
		{
			function_parens++;
			/* This is a bit ugly but it avoids using a temporary in the Lisp */
			zprintf("(setf (values %s %s) (apply #'values ", show_hardreg(dest1), show_hardreg(dest2));
		}
		else
		{
			zprintf("(setf %s ", show_hardreg(dest1));
		}
	}
	zprintf("(funcall %s", show_hardreg(func));
}

static void cg_call_arg(struct hardreg* arg)
{
	zprintf(" %s", show_hardreg(arg));
}

static void cg_call_end(void)
{
	while (function_parens)
	{
		function_parens--;
		zprintf(")");
	}
	zprintf("\n");
}

/* Return */
static void cg_ret(struct hardreg *reg1, struct hardreg *reg2)
{
	if (reg1)
		if (reg2)
			zprintf("(return (list %s %s))\n", show_hardreg(reg1), show_hardreg(reg2));
		else
			zprintf("(return %s)\n", show_hardreg(reg1));
	else
		zprintf("(return)\n");
}

/* Do a structure copy from one location to another. */

static void cg_memcpy(struct hardregref* src, struct hardregref* dest, int size)
{
	// FIXME
	assert(src->type == TYPE_PTR);
	assert(dest->type == TYPE_PTR);

	zprintf("(_memcpy sp stack %s %s %s %s %d)\n",
			show_hardreg(dest->simple),
			show_hardreg(dest->base),
			show_hardreg(src->simple),
			show_hardreg(src->base),
			size);
}


const struct codegenerator cg_lisp =
{
	.pointer_zero_offset = 0,
	.spname = "sp",
	.fpname = "fp",
	.stackname = "stack",

	.register_class =
	{
		[0] = REGTYPE_ALL,
	},
	.reset_registers = cg_reset_registers,
	.init_register = cg_init_register,
	.get_register_name = cg_get_register_name,

	.prologue = cg_prologue,
	.epilogue = cg_epilogue,
	.comment = cg_comment,

	.declare_function = cg_declare_function,
	.declare_function_arg = cg_declare_function_arg,
	.declare_function_vararg = cg_declare_function_vararg,
	.declare_function_end = cg_declare_function_end,

	.declare_slot = cg_declare_slot,
	.create_storage = cg_create_storage,
	.import = cg_import,
	.export = cg_export,

	.function_prologue = cg_function_prologue,
	.function_prologue_arg = cg_function_prologue_arg,
	.function_prologue_vararg = cg_function_prologue_vararg,
	.function_prologue_reg = cg_function_prologue_reg,
	.function_prologue_end = cg_function_prologue_end,

	.function_epilogue = cg_function_epilogue,

	.bb_start = cg_bb_start,
	.bb_end_jump = cg_bb_end_jump,
	.bb_end_if_arith = cg_bb_end_if,
	.bb_end_if_ptr = cg_bb_end_if,

	.copy = cg_copy,
	.load = cg_load,
	.store = cg_store,

	.set_int = cg_set_int,
	.set_float = cg_set_float,
	.set_osymbol = cg_set_symbol,
	.set_fsymbol = cg_set_symbol,

	.toint = cg_toint,
	.negate = cg_negate,
	.add = cg_add,
	.subtract = cg_subtract,
	.multiply = cg_multiply,
	.divide = cg_divide,
	.mod = cg_mod,
	.shl = cg_shl,
	.shr = cg_shr,
	.logand = cg_logand,
	.logor = cg_logor,
	.logxor = cg_logxor,
	.booland = cg_booland,
	.boolor = cg_boolor,
	.set_gt = cg_set_gt,
	.set_ge = cg_set_ge,
	.set_lt = cg_set_lt,
	.set_le = cg_set_le,
	.set_eq = cg_set_eq,
	.set_ne = cg_set_ne,

	.select_ptr = cg_select,
	.select_arith = cg_select,

	.call = cg_call,
	.call_arg = cg_call_arg,
	.call_end = cg_call_end,

	.ret = cg_ret,

	.memcpy = cg_memcpy
};
