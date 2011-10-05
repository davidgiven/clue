/* cg-c.c
 * Backend for C
 *
 * © 2008 David Given.
 * Clue is licensed under the Revised BSD open source license. To get the
 * full license text, see the README file.
 *
 * $Id$
 * $HeadURL$
 * $LastChangedDate: 2007-04-30 22:41:42 +0000 (Mon, 30 Apr 2007) $
 */

#include "globals.h"
#define CLUE_EMULATE_INT_WITH_DOUBLE

enum
{
	MAX_CALL_ARGS = 64,

	CALLTYPE_VOID,
	CALLTYPE_ARITH,
	CALLTYPE_PTR,
};

static int function_arg_list = 0;
static struct hardreg* call_return_reg1;
static struct hardreg* call_return_reg2;
static struct hardreg* call_function_reg;
static int call_arg_count;
static int call_real_arg_count;
static struct hardreg* call_arg[MAX_CALL_ARGS];
static int register_count;

enum
{
	REGCLASS_FLOAT,
	REGCLASS_BOOL,
	REGCLASS_OPTR,
	REGCLASS_FPTR,
#if !defined CLUE_EMULATE_INT_WITH_DOUBLE
	REGCLASS_INT
#endif
};

static const struct
{
	const char* prefix;
	const char* suffix;
	const char* type;
} regclassdata[] =
{
	[REGCLASS_FLOAT] =
	{
		.prefix = "FLOAT",
		.suffix = "f",
		.type = "clue_real_t"
	},

	[REGCLASS_BOOL] =
	{
		.prefix = "BOOL",
		.suffix = "b",
		.type = "clue_bool_t"
	},

	[REGCLASS_OPTR] =
	{
		.prefix = "OPTR",
		.suffix = "o",
		.type = "clue_optr_t"
	},

	[REGCLASS_FPTR] =
	{
		.prefix = "FPTR",
		.suffix = "p",
		.type = "clue_fptr_t"
	},

#if !defined CLUE_EMULATE_INT_WITH_DOUBLE
	[REGCLASS_INT] =
	{
		.prefix = "INT",
		.suffix = "i",
		.type = "clue_int_t"
	},
#endif
};

/* Reset the register tracking. */

static void cg_reset_registers(void)
{
	register_count = 0;
}

/* Initialize a new hardreg. */

static void cg_init_register(struct hardreg* reg, int regclass)
{
	assert(!reg->name);
	reg->name = aprintf("%s%d", regclassdata[regclass].prefix,
			register_count);
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
	zprintf("#include <clue-crt.h>\n");
}

/* Emit the file epilogue. */

static void cg_epilogue(void)
{
}

/* Emit a comment (contains no actual code). */

static void cg_comment(const char* format, ...)
{
	if (verbose)
	{
		va_list ap;
		va_start(ap, format);
		zprintf("// ");
		zvprintf(format, ap);
		va_end(ap);
	}
}

static void cg_declare_function(struct symbol* sym, int returning)
{
	switch (returning)
	{
		case REGCLASS_VOID:
			zprintf("void ");
			break;

		case REGCLASS_REGPAIR:
			zprintf("clue_ptr_pair_t ");
			break;

		default:
			zprintf("%s ", regclassdata[returning].type);
			break;
	}

	zprintf("%s(", show_symbol_mangled(sym));
	function_arg_list = 0;
}

static void cg_declare_function_arg(int regclass)
{
	if (function_arg_list > 0)
		zprintf(", ");
	zprintf("%s", regclassdata[regclass].type);
	function_arg_list++;
}

static void cg_declare_function_vararg()
{
	if (function_arg_list > 0)
		zprintf(", ");
	zprintf("...");
	function_arg_list++;
}

static void cg_declare_function_end(void)
{
	zprintf(");\n");
}

static void cg_declare_slot(struct symbol* sym, unsigned size)
{
	zprintf("clue_slot_t %s[%d];\n", show_symbol_mangled(sym), size);
}

static void cg_create_storage(struct symbol* sym, unsigned size)
{
}

static void cg_import(struct symbol* sym)
{
}

static void cg_export(struct symbol* sym)
{
}

static void cg_function_prologue(struct symbol* sym, int returning)
{
	if (!sym)
	{
		zprintf("static void clue_initializer(void) CLUE_CONSTRUCTOR;\n");
		zprintf("static void clue_initializer(");
	}
	else
	{
		switch (returning)
		{
			case REGCLASS_VOID:
				zprintf("void ");
				break;

			case REGCLASS_REGPAIR:
				zprintf("clue_ptr_pair_t ");
				break;

			default:
				zprintf("%s ", regclassdata[returning].type);
				break;

		}

		if (!sym)
		{
			zprintf("clue_initializer(");
		}
		else
		{
			zprintf("%s(", show_symbol_mangled(sym));
		}
	}

	function_arg_list = 0;
}

static void cg_function_prologue_arg(struct hardreg* reg)
{
	if (function_arg_list > 0)
		zprintf(", ");
	zprintf("%s %s", regclassdata[reg->regclass].type, show_hardreg(reg));
	function_arg_list++;
}

static void cg_function_prologue_vararg(void)
{
	if (function_arg_list > 0)
		zprintf(", ");
	zprintf("...");
	function_arg_list++;
}

static void cg_function_prologue_reg(struct hardreg* reg)
{
	if (function_arg_list != -1)
	{
		zprintf(") {\n");
		function_arg_list = -1;
	}
	zprintf("%s %s;\n", regclassdata[reg->regclass].type, show_hardreg(reg));
}

static void cg_function_prologue_end(void)
{
}

static void cg_function_epilogue(void)
{
	zprintf("}\n\n");
}

/* Starts a basic block. */

static void cg_bb_start(struct binfo* binfo)
{
	if (binfo->id != 0)
		zprintf("LABEL%d:\n", binfo->id);
}

/* Ends a basic block in an unconditional jump. */

static void cg_bb_end_jump(struct binfo* target)
{
	zprintf("goto LABEL%d;\n", target->id);
}

/* Ends a basic block in a conditional jump based on any value. */

static void cg_bb_end_if(struct hardreg* cond,
		struct binfo* truetarget, struct binfo* falsetarget)
{
	zprintf("if (%s) goto LABEL%d; else goto LABEL%d;\n",
			show_hardreg(cond), truetarget->id, falsetarget->id);
}

/* Copies a single register. */

static void cg_copy(struct hardreg* src, struct hardreg* dest)
{
	if (src != dest)
		zprintf("%s = %s;\n", show_hardreg(dest), show_hardreg(src));
}

/* Loads a value from a memory location. */

static void cg_load(struct hardreg* simple, struct hardreg* base,
		int offset, struct hardreg* dest)
{
	zprintf("%s = %s[(int)%s + %d].%s;\n",
			show_hardreg(dest),
			show_hardreg(base),
			show_hardreg(simple),
			offset,
			regclassdata[dest->regclass].suffix);
}

/* Stores a value from a memory location. */

static void cg_store(struct hardreg* simple, struct hardreg* base,
		int offset, struct hardreg* src)
{
	if (simple)
	{
		zprintf("%s[(int)%s + %d].%s = %s;\n",
				show_hardreg(base),
				show_hardreg(simple),
				offset,
				regclassdata[src->regclass].suffix,
				show_hardreg(src));
	}
	else
	{
		zprintf("%s[%d].%s = %s;\n",
				show_hardreg(base),
				offset,
				regclassdata[src->regclass].suffix,
				show_hardreg(src));
	}
}

/* Load a constant int. */

static void cg_set_int(long long int value, struct hardreg* dest)
{
	zprintf("%s = %lld;\n", show_hardreg(dest), value);
}

/* Load a constant float. */

static void cg_set_float(long double value, struct hardreg* dest)
{
	zprintf("%s = %llf;\n", show_hardreg(dest), value);
}

/* Load a constant symbol. */

static void cg_set_symbol(struct symbol* sym, struct hardreg* dest)
{
	zprintf("%s = %s%s;\n", show_hardreg(dest),
			(dest->regclass == REGCLASS_FPTR) ? "(clue_fptr_t)" : "",
			sym ? show_symbol_mangled(sym) : "NULL");
}

/* Convert to integer. */

static void cg_toint(struct hardreg* src, struct hardreg* dest)
{
	zprintf("%s = (int) %s;\n", show_hardreg(dest), show_hardreg(src));
}

/* Arithmetic negation. */

static void cg_negate(struct hardreg* src, struct hardreg* dest)
{
	zprintf("%s = -%s;\n", show_hardreg(dest), show_hardreg(src));
}

#define SIMPLE_INFIX_2OP(NAME, OP, CAST) \
	static void cg_##NAME(struct hardreg* src1, struct hardreg* src2, \
			struct hardreg* dest) \
	{ \
		zprintf("%s = " CAST " %s " OP " " CAST " %s;\n", show_hardreg(dest), \
				show_hardreg(src1), show_hardreg(src2)); \
	}

SIMPLE_INFIX_2OP(add, "+", "")
SIMPLE_INFIX_2OP(subtract, "-", "")
SIMPLE_INFIX_2OP(multiply, "*", "")
SIMPLE_INFIX_2OP(divide, "/", "")
SIMPLE_INFIX_2OP(mod, "%%", "(clue_realint_t)")
SIMPLE_INFIX_2OP(logand, "&", "(clue_realint_t)")
SIMPLE_INFIX_2OP(logor, "|", "(clue_realint_t)")
SIMPLE_INFIX_2OP(logxor, "^", "(clue_realint_t)")
SIMPLE_INFIX_2OP(shl, "<<", "(clue_realint_t)")
SIMPLE_INFIX_2OP(shr, ">>", "(clue_realint_t)")

#define SIMPLE_SET_2OP(NAME, OP, CAST) \
	static void cg_##NAME(struct hardreg* src1, struct hardreg* src2, \
			struct hardreg* dest) \
	{ \
		zprintf("%s = (" CAST " %s " OP " " CAST " %s) ? 1 : 0;\n", \
				show_hardreg(dest), \
				show_hardreg(src1), show_hardreg(src2)); \
	}

SIMPLE_SET_2OP(booland, "&&", "(clue_realint_t)")
SIMPLE_SET_2OP(boolor, "||", "(clue_realint_t)")
SIMPLE_SET_2OP(set_gt, ">", "")
SIMPLE_SET_2OP(set_ge, ">=", "")
SIMPLE_SET_2OP(set_lt, "<", "")
SIMPLE_SET_2OP(set_le, "<=", "")
SIMPLE_SET_2OP(set_eq, "==", "")
SIMPLE_SET_2OP(set_ne, "!=", "")

/* Select operations using any condition. */

static void cg_select(struct hardreg* cond,
		struct hardreg* dest1, struct hardreg* dest2,
		struct hardreg* true1, struct hardreg* true2,
		struct hardreg* false1, struct hardreg* false2)
{
	zprintf("if (%s) ", show_hardreg(cond));
	if (dest2)
		zprintf("{ %s = %s; %s = %s; } else { %s = %s; %s = %s; }",
				show_hardreg(dest1), show_hardreg(true1),
				show_hardreg(dest2), show_hardreg(true2),
				show_hardreg(dest1), show_hardreg(false1),
				show_hardreg(dest2), show_hardreg(false2));
	else
		zprintf("{ %s = %s; } else { %s = %s; }",
				show_hardreg(dest1), show_hardreg(true1),
				show_hardreg(dest1), show_hardreg(false1));
	zprintf("\n");
}

static void cg_call(struct hardreg* func,
		struct hardreg* dest1, struct hardreg* dest2)
{
	call_function_reg = func;
	call_arg_count = 0;
	call_real_arg_count = -1;
	call_return_reg1 = dest1;
	call_return_reg2 = dest2;
}

static void cg_call_arg(struct hardreg* arg)
{
	call_arg[call_arg_count] = arg;
	call_arg_count++;
}

static void cg_call_vararg(struct hardreg* arg)
{
	if (call_real_arg_count == -1)
		call_real_arg_count = call_arg_count;

	call_arg[call_arg_count] = arg;
	call_arg_count++;
}

static void cg_call_end(void)
{
	if (call_real_arg_count == -1)
		call_real_arg_count = call_arg_count;

	if (call_return_reg1 && call_return_reg2)
		zprintf("{ clue_ptr_pair_t _r = ");
	else if (call_return_reg1)
		zprintf("%s = ", show_hardreg(call_return_reg1));

	/* Emit a cast turning the clue_fptr_t into a function of the right type.
	 * */

	zprintf("((");
	if (call_return_reg1 && call_return_reg2)
		zprintf("clue_ptr_pair_t");
	else if (call_return_reg1)
		zprintf("%s", regclassdata[call_return_reg1->regclass].type);
	else
		zprintf("void");
	zprintf(" (*)(");

	if (call_arg_count == 0)
		zprintf("void");
	else
	{
		int i = 0;
		for (i = 0; i < call_real_arg_count; i++)
		{
			if (i > 0)
				zprintf(", ");
			zprintf("%s", regclassdata[call_arg[i]->regclass].type);
		}

		if (call_real_arg_count < call_arg_count)
		{
			if (i > 0)
				zprintf(", ");
			zprintf("...");
		}
	}

	zprintf("))");

	/* ...the function pointer... */

	zprintf("%s)", show_hardreg(call_function_reg));

	/* ...and the arguments. */

	zprintf("(");
	{
		int i;
		for (i = 0; i < call_arg_count; i++)
		{
			if (i > 0)
				zprintf(", ");
			zprintf("%s", show_hardreg(call_arg[i]));
		}
	}
	zprintf(")");

	/* Now the call epilogue. */

	if (call_return_reg1 && call_return_reg2)
	{
		zprintf("; ");
		zprintf("%s = _r.i;\n", show_hardreg(call_return_reg1));
		zprintf("%s = _r.o;\n", show_hardreg(call_return_reg2));
		zprintf("}");
	}

	zprintf(";\n");
}

/* Return a pointer. */

static void cg_ret(struct hardreg* reg1, struct hardreg* reg2)
{
	if (reg1)
		if (reg2)
			zprintf("{ clue_ptr_pair_t _r; _r.i = %s; _r.o = %s; return _r; }\n",
						show_hardreg(reg1),
						show_hardreg(reg2));
		else
			zprintf("return %s;\n",
					show_hardreg(reg1));
	else
		zprintf("return;\n");
}

/* Do a structure copy from one location to another. */

static void cg_memcpy(struct hardregref* src, struct hardregref* dest, int size)
{
	assert(src->type == TYPE_PTR);
	assert(dest->type == TYPE_PTR);

	zprintf("_memcpy(sp, stack, %s, %s, %s, %s, %d);\n",
			show_hardreg(dest->simple),
			show_hardreg(dest->base),
			show_hardreg(src->simple),
			show_hardreg(src->base),
			size);
}


const struct codegenerator cg_c =
{
	.pointer_zero_offset = 0,
	.spname = "sp",
	.fpname = "fp",
	.stackname = "stack",

	.register_class =
	{
#if defined CLUE_EMULATE_INT_WITH_DOUBLE
		[REGCLASS_FLOAT] = REGTYPE_INT | REGTYPE_FLOAT,
#else
		[REGCLASS_INT] = REGTYPE_INT,
		[REGCLASS_FLOAT] = REGTYPE_FLOAT,
#endif
		[REGCLASS_BOOL] = REGTYPE_BOOL,
		[REGCLASS_OPTR] = REGTYPE_OPTR,
		[REGCLASS_FPTR] = REGTYPE_FPTR
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
	.call_vararg = cg_call_vararg,
	.call_end = cg_call_end,

	.ret = cg_ret,

	.memcpyimpl = cg_memcpy
};
