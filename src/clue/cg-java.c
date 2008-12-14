/* cg-java.c
 * Backend for Java
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

enum
{
	MAX_CALL_ARGS = 64,

	CALLTYPE_VOID,
	CALLTYPE_ARITH,
	CALLTYPE_PTR,
};

static int function_is_initialiser = 0;
static int function_arg_list = 0;
static struct hardreg* call_return_reg1;
static struct hardreg* call_return_reg2;
static struct hardreg* call_function_reg;
static int call_arg_count;
static int register_count;

enum
{
	REGCLASS_FLOAT,
	REGCLASS_BOOL,
	REGCLASS_OPTR,
	REGCLASS_FPTR,
	REGCLASS_INT
};

static const struct
{
	const char* prefix;
	const char* type;
	const char* memtype;
	const char* accessor;
	const char* example;
} regclassdata[] =
{
	[REGCLASS_FLOAT] =
	{
		.prefix = "FLOAT",
		.type = "double",
		.memtype = "double",
		.accessor = "doubledata",
		.example = "0",
	},

	[REGCLASS_BOOL] =
	{
		.prefix = "BOOL",
		.type = "boolean",
		.memtype = "boolean",
		.accessor = "booleandata",
		.example = "false",
	},

	[REGCLASS_OPTR] =
	{
		.prefix = "OPTR",
		.type = "ClueMemory",
		.memtype = "ClueMemory",
		.accessor = "objectdata",
		.example = "null",
	},

	[REGCLASS_FPTR] =
	{
		.prefix = "FPTR",
		.type = "ClueRunnable",
		.memtype = "ClueRunnable",
		.accessor = "functiondata",
		.example = "null",
	},

	/* A sparse bug means we have to use the same types for ints and doubles.
	 */

	[REGCLASS_INT] =
	{
		.prefix = "INT",
		.type = "double",
		.memtype = "double",
		.accessor = "doubledata",
		.example = "0",
	},
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
}

static void cg_declare_function_arg(int regclass)
{
}

static void cg_declare_function_vararg()
{
}

static void cg_declare_function_end(void)
{
}

static void cg_declare_slot(struct symbol* sym, unsigned size)
{
	zprintf("public static ClueMemory %s;\n", show_symbol_mangled(sym));
}

static void cg_create_storage(struct symbol* sym, unsigned size)
{
	zprintf("%s = new ClueMemory(%u);\n", show_symbol_mangled(sym), size);
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
		zprintf("static {\n");

		function_is_initialiser = 1;
	}
	else
	{
		const char* symname = show_symbol_mangled(sym);
		zprintf("public static ClueRunnable %s = new ClueRunnable() {\n",
				symname);
		zprintf("public void run() {\n");

		function_is_initialiser = 0;
	}

	function_arg_list = 0;
}

static void cg_function_prologue_arg(struct hardreg* reg)
{
	zprintf("%s %s = (%s) args.%s[%d];\n",
		regclassdata[reg->regclass].type,
		show_hardreg(reg),
		regclassdata[reg->regclass].type,
		regclassdata[reg->regclass].accessor,
		function_arg_list);

	function_arg_list++;
}

static void cg_function_prologue_vararg(void)
{
	if (function_arg_list > 0)
		zprintf(", ");
	zprintf("*** varargs not supported yet***\n");
	function_arg_list++;
}

static void cg_function_prologue_reg(struct hardreg* reg)
{
	zprintf("%s %s = %s;\n",
			regclassdata[reg->regclass].type,
			show_hardreg(reg),
			regclassdata[reg->regclass].example);
}

static void cg_function_prologue_end(void)
{
	zprintf("int state = 0;\n");
	zprintf("stateloop: for (;;) {\n");
	zprintf("switch (state) {\n");
	zprintf("case 0:\n");
}

static void cg_function_epilogue(void)
{
	if (function_is_initialiser)
		zprintf("}}}\n");
	else
		zprintf("}}}};\n\n");
}

/* Starts a basic block. */

static void cg_bb_start(struct binfo* binfo)
{
	if (binfo->id != 0)
		zprintf("case %d:\n", binfo->id);
}

/* Ends a basic block in an unconditional jump. */

static void cg_bb_end_jump(struct binfo* target)
{
	zprintf("state = %d; break;\n", target->id);
}

/* Ends a basic block in a conditional jump based on an arithmetic value. */

static void cg_bb_end_if_arith(struct hardreg* cond,
		struct binfo* truetarget, struct binfo* falsetarget)
{
	zprintf("state = (%s != 0) ? %d : %d; break;\n",
			show_hardreg(cond), truetarget->id, falsetarget->id);
}

/* Ends a basic block in a conditional jump based on a pointer value. */

static void cg_bb_end_if_ptr(struct hardreg* cond,
		struct binfo* truetarget, struct binfo* falsetarget)
{
	zprintf("state = (%s != null) ? %d : %d; break;\n",
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
	zprintf("%s = (%s) %s.%s[(int)%s + %d];\n",
			show_hardreg(dest),
			regclassdata[dest->regclass].type,
			show_hardreg(base),
			regclassdata[dest->regclass].accessor,
			show_hardreg(simple),
			offset);
}

/* Stores a value from a memory location. */

static void cg_store(struct hardreg* simple, struct hardreg* base,
		int offset, struct hardreg* src)
{
	if (simple)
	{
		zprintf("%s.%s[(int)%s + %d] = (%s) %s;\n",
				show_hardreg(base),
				regclassdata[src->regclass].accessor,
				show_hardreg(simple),
				offset,
				regclassdata[src->regclass].memtype,
				show_hardreg(src));
	}
	else
	{
		zprintf("%s.%s[%d] = (%s) %s;\n",
				show_hardreg(base),
				regclassdata[src->regclass].accessor,
				offset,
				regclassdata[src->regclass].memtype,
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
	if (!sym)
	{
		zprintf("%s = null;\n", show_hardreg(dest));
	}
	else
	{
		zprintf("%s = %s;\n", show_hardreg(dest),
				show_symbol_mangled(sym));
	}
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
SIMPLE_INFIX_2OP(mod, "%%", "(long)")
SIMPLE_INFIX_2OP(logand, "&", "(long)")
SIMPLE_INFIX_2OP(logor, "|", "(long)")
SIMPLE_INFIX_2OP(logxor, "^", "(long)")
SIMPLE_INFIX_2OP(shl, "<<", "(long)")
SIMPLE_INFIX_2OP(shr, ">>", "(long)")

#define SIMPLE_SET_2OP(NAME, OP, CAST) \
	static void cg_##NAME(struct hardreg* src1, struct hardreg* src2, \
			struct hardreg* dest) \
	{ \
		zprintf("%s = (" CAST " %s " OP " " CAST " %s) ? 1 : 0;\n", \
				show_hardreg(dest), \
				show_hardreg(src1), show_hardreg(src2)); \
	}

SIMPLE_SET_2OP(set_gt, ">", "")
SIMPLE_SET_2OP(set_ge, ">=", "")
SIMPLE_SET_2OP(set_lt, "<", "")
SIMPLE_SET_2OP(set_le, "<=", "")
SIMPLE_SET_2OP(set_eq, "==", "")
SIMPLE_SET_2OP(set_ne, "!=", "")

static void cg_booland(struct hardreg* src1, struct hardreg* src2, \
		struct hardreg* dest) \
{ \
	zprintf("%s = ((%s != 0) && (%s != 0)) ? 1 : 0;\n", \
			show_hardreg(dest), \
			show_hardreg(src1), show_hardreg(src2)); \
}

static void cg_boolor(struct hardreg* src1, struct hardreg* src2, \
		struct hardreg* dest) \
{ \
	zprintf("%s = ((%s != 0) || (%s != 0)) ? 1 : 0;\n", \
			show_hardreg(dest), \
			show_hardreg(src1), show_hardreg(src2)); \
}

/* Select operations. */

static void cg_select(struct hardreg* cond,
		struct hardreg* dest1, struct hardreg* dest2,
		struct hardreg* true1, struct hardreg* true2,
		struct hardreg* false1, struct hardreg* false2,
		const char* condition)
{
	zprintf("if (%s != %s) ", show_hardreg(cond), condition);
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

/* Select operations using a pointer. */

static void cg_select_ptr(struct hardreg* cond,
		struct hardreg* dest1, struct hardreg* dest2,
		struct hardreg* true1, struct hardreg* true2,
		struct hardreg* false1, struct hardreg* false2)
{
	cg_select(cond, dest1, dest2, true1, true2, false1, false2,
			"null");
}

/* Select operations using an arithmetic value. */

static void cg_select_arith(struct hardreg* cond,
		struct hardreg* dest1, struct hardreg* dest2,
		struct hardreg* true1, struct hardreg* true2,
		struct hardreg* false1, struct hardreg* false2)
{
	cg_select(cond, dest1, dest2, true1, true2, false1, false2,
			"0");
}

static void cg_call(struct hardreg* func,
		struct hardreg* dest1, struct hardreg* dest2)
{
	call_function_reg = func;
	call_arg_count = 0;
	call_return_reg1 = dest1;
	call_return_reg2 = dest2;
}

static void cg_call_arg(struct hardreg* arg)
{
	zprintf("args.%s[%u] = (%s) %s;\n",
			regclassdata[arg->regclass].accessor,
			call_arg_count,
			regclassdata[arg->regclass].memtype,
			show_hardreg(arg));

	call_arg_count++;
}

static void cg_call_end(void)
{
	/* The function call... */

	zprintf("%s.run();\n", show_hardreg(call_function_reg));

	/* Now the call epilogue. */

	if (call_return_reg1)
		zprintf("%s = (%s) args.%s[0];\n",
				show_hardreg(call_return_reg1),
				regclassdata[call_return_reg1->regclass].type,
				regclassdata[call_return_reg1->regclass].accessor);
	if (call_return_reg2)
		zprintf("%s = (%s) args.%s[1];\n",
				show_hardreg(call_return_reg2),
				regclassdata[call_return_reg2->regclass].type,
				regclassdata[call_return_reg2->regclass].accessor);
}

/* Return a pointer. */

static void cg_ret(struct hardreg* reg1, struct hardreg* reg2)
{
	if (reg1)
		zprintf("args.%s[0] = (%s) %s;\n",
				regclassdata[reg1->regclass].accessor,
				regclassdata[reg1->regclass].memtype,
				show_hardreg(reg1));
	if (reg2)
		zprintf("args.%s[1] = (%s) %s;\n",
				regclassdata[reg2->regclass].accessor,
				regclassdata[reg2->regclass].memtype,
				show_hardreg(reg2));

	if (function_is_initialiser)
		zprintf("break stateloop;\n");
	else
		zprintf("return;\n");
}

/* Do a structure copy from one location to another. */

static void cg_memcpy(struct hardregref* src, struct hardregref* dest, int size)
{
	assert(src->type == TYPE_PTR);
	assert(dest->type == TYPE_PTR);

	zprintf("args.doubledata[0] = sp;\n");
	zprintf("args.objectdata[1] = stack;\n");
	zprintf("args.doubledata[2] = (double) %s;\n",
			show_hardreg(dest->simple));
	zprintf("args.objectdata[3] = %s;\n",
			show_hardreg(dest->base));
	zprintf("args.doubledata[4] = (double) %s;\n",
			show_hardreg(src->simple));
	zprintf("args.objectdata[5] = %s;\n",
			show_hardreg(src->base));
	zprintf("args.doubledata[5] = (double) %d;\n",
			size);
	zprintf("_memcpy.run();\n");
}


const struct codegenerator cg_java =
{
	.pointer_zero_offset = 0,
	.spname = "sp",
	.fpname = "fp",
	.stackname = "stack",

	.register_class =
	{
		[REGCLASS_INT] = REGTYPE_INT,
		[REGCLASS_FLOAT] = REGTYPE_FLOAT,
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
	.bb_end_if_arith = cg_bb_end_if_arith,
	.bb_end_if_ptr = cg_bb_end_if_ptr,

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

	.select_ptr = cg_select_ptr,
	.select_arith = cg_select_arith,

	.call = cg_call,
	.call_arg = cg_call_arg,
	.call_vararg = cg_call_arg,
	.call_end = cg_call_end,

	.ret = cg_ret,

	.memcpy = cg_memcpy
};
