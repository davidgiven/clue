/* cg-lua.c
 * Backend for Lua
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

static int function_arg_list = 0;
static int function_is_initializer = 0;
static int register_count;

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
	zprintf("require \"clue.crt\"\n");
	zprintf("local int = clue.crt.int\n");
	zprintf("local booland = clue.crt.booland\n");
	zprintf("local boolor = clue.crt.boolor\n");
	zprintf("local logand = clue.crt.logand\n");
	zprintf("local logor = clue.crt.logor\n");
	zprintf("local logxor = clue.crt.logxor\n");
	zprintf("local lognot = clue.crt.lognot\n");
	zprintf("local shl = clue.crt.shl\n");
	zprintf("local shr = clue.crt.shr\n");
	zprintf("local _memcpy = _memcpy\n");
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
		zprintf("-- ");
		zvprintf(format, ap);
		va_end(ap);
	}
}

static void cg_declare_slot(struct symbol* sym, unsigned size)
{
	zprintf("local %s\n", show_symbol_mangled(sym));
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
	zprintf("%s = {}\n", show_symbol_mangled(sym));
}

static void cg_import(struct symbol* sym)
{
	const char* s = show_symbol_mangled(sym);
	zprintf("%s = _G.%s\n", s, s);
}

static void cg_export(struct symbol* sym)
{
	const char* s = show_symbol_mangled(sym);
	zprintf("_G.%s = %s\n", s, s);
}

static void cg_function_prologue(struct symbol* sym, int returning)
{
	if (!sym)
	{
		zprintf("local function initializer(");
		function_is_initializer = 1;
	}
	else
	{
		zprintf("%s = function(", show_symbol_mangled(sym));
		function_is_initializer = 0;
	}

	function_arg_list = 0;
}

static void cg_function_prologue_arg(struct hardreg* reg)
{
	if (function_arg_list > 0)
		zprintf(", ");
	zprintf("%s", show_hardreg(reg));
	function_arg_list++;
}

static void cg_function_prologue_vararg(void)
{
	if (function_arg_list > 0)
		zprintf(", ");
	zprintf("...");
}

static void cg_function_prologue_reg(struct hardreg* reg)
{
	if (function_arg_list != -1)
	{
		zprintf(")\n");
		function_arg_list = -1;
	}
	zprintf("local %s\n", show_hardreg(reg));
}

static void cg_function_prologue_end(void)
{
	zprintf("local state = 0;\n");
	zprintf("while true do\n");
	zprintf("repeat\n");
	zprintf("if state == 0 then\n");
}

static void cg_function_epilogue(void)
{
	zprintf("until true\n");
	zprintf("end\n");
	zprintf("end\n\n");
	if (function_is_initializer)
		zprintf("clue.crt.add_initializer(initializer)\n");
}

/* Starts a basic block. */

static void cg_bb_start(struct binfo* binfo)
{
	if (binfo->id != 0)
		zprintf("if state == %d then\n", binfo->id);
}

/* Ends a basic block in an unconditional jump. */

static void cg_bb_end_jump(struct binfo* target)
{
	zprintf("state = %d break end\n", target->id);
}

/* Ends a basic block in a conditional jump based on an arithmetic value. */

static void cg_bb_end_if_arith(struct hardreg* cond,
		struct binfo* truetarget, struct binfo* falsetarget)
{
	zprintf("if %s ~= 0 then state = %d else state = %d end break end\n",
			show_hardreg(cond), truetarget->id, falsetarget->id);
}

/* Ends a basic block in a conditional jump based on a pointer base value. */

static void cg_bb_end_if_ptr(struct hardreg* cond,
		struct binfo* truetarget, struct binfo* falsetarget)
{
	zprintf("if %s then state = %d else state = %d end break end\n",
			show_hardreg(cond), truetarget->id, falsetarget->id);
}

/* Copies a single register. */

static void cg_copy(struct hardreg* src, struct hardreg* dest)
{
	if (src != dest)
		zprintf("%s = %s\n", show_hardreg(dest), show_hardreg(src));
}

/* Loads a value from a memory location. */

static void cg_load(struct hardreg* simple, struct hardreg* base,
		int offset, struct hardreg* dest)
{
	zprintf("%s = %s[%s + %d]\n",
			show_hardreg(dest),
			show_hardreg(base),
			show_hardreg(simple),
			offset);
}

/* Stores a value from a memory location. */

static void cg_store(struct hardreg* simple, struct hardreg* base,
		int offset, struct hardreg* src)
{
	if (simple)
	{
		zprintf("%s[%s + %d] = %s\n",
				show_hardreg(base),
				show_hardreg(simple),
				offset,
				show_hardreg(src));
	}
	else
	{
		zprintf("%s[%d] = %s\n",
				show_hardreg(base),
				offset,
				show_hardreg(src));
	}
}

/* Load a constant int. */

static void cg_set_int(long long int value, struct hardreg* dest)
{
	zprintf("%s = %lld\n", show_hardreg(dest), value);
}

/* Load a constant float. */

static void cg_set_float(long double value, struct hardreg* dest)
{
	zprintf("%s = %llf\n", show_hardreg(dest), value);
}

/* Load a constant symbol. */

static void cg_set_symbol(struct symbol* sym, struct hardreg* dest)
{
	zprintf("%s = %s\n", show_hardreg(dest),
			sym ? show_symbol_mangled(sym) : "nil");
}

/* Convert to integer. */

static void cg_toint(struct hardreg* src, struct hardreg* dest)
{
	zprintf("%s = int(%s)\n", show_hardreg(dest), show_hardreg(src));
}

/* Arithmetic negation. */

static void cg_negate(struct hardreg* src, struct hardreg* dest)
{
	zprintf("%s = -%s\n", show_hardreg(dest), show_hardreg(src));
}

#define SIMPLE_INFIX_2OP(NAME, OP) \
	static void cg_##NAME(struct hardreg* src1, struct hardreg* src2, \
			struct hardreg* dest) \
	{ \
		zprintf("%s = %s " OP " %s\n", show_hardreg(dest), \
				show_hardreg(src1), show_hardreg(src2)); \
	}

SIMPLE_INFIX_2OP(add, "+")
SIMPLE_INFIX_2OP(subtract, "-")
SIMPLE_INFIX_2OP(multiply, "*")
SIMPLE_INFIX_2OP(divide, "/")
SIMPLE_INFIX_2OP(mod, "%%")

#define SIMPLE_PREFIX_2OP(NAME, OP) \
	static void cg_##NAME(struct hardreg* src1, struct hardreg* src2, \
			struct hardreg* dest) \
	{ \
		zprintf("%s = " OP "(%s, %s)\n", show_hardreg(dest), \
				show_hardreg(src1), show_hardreg(src2)); \
	}

SIMPLE_PREFIX_2OP(logand, "logand")
SIMPLE_PREFIX_2OP(logor, "logor")
SIMPLE_PREFIX_2OP(logxor, "logxor")
SIMPLE_PREFIX_2OP(booland, "booland")
SIMPLE_PREFIX_2OP(boolor, "boolor")
SIMPLE_PREFIX_2OP(shl, "shl")
SIMPLE_PREFIX_2OP(shr, "shr")

#define SIMPLE_SET_2OP(NAME, OP) \
	static void cg_##NAME(struct hardreg* src1, struct hardreg* src2, \
			struct hardreg* dest) \
	{ \
		zprintf("%s = %s " OP " %s and 1 or 0\n", show_hardreg(dest), \
				show_hardreg(src1), show_hardreg(src2)); \
	}

SIMPLE_SET_2OP(set_gt, ">")
SIMPLE_SET_2OP(set_ge, ">=")
SIMPLE_SET_2OP(set_lt, "<")
SIMPLE_SET_2OP(set_le, "<=")
SIMPLE_SET_2OP(set_eq, "==")
SIMPLE_SET_2OP(set_ne, "~=")

/* Select operations using an arithmetic condition. */

static void cg_select_arith(struct hardreg* cond,
		struct hardreg* dest1, struct hardreg* dest2,
		struct hardreg* true1, struct hardreg* true2,
		struct hardreg* false1, struct hardreg* false2)
{
	zprintf("if %s ~= 0 then ", show_hardreg(cond));
	if (dest2)
		zprintf("%s = %s %s = %s else %s = %s %s = %s",
				show_hardreg(dest1), show_hardreg(true1),
				show_hardreg(dest2), show_hardreg(true2),
				show_hardreg(dest1), show_hardreg(false1),
				show_hardreg(dest2), show_hardreg(false2));
	else
		zprintf("%s = %s else %s = %s",
				show_hardreg(dest1), show_hardreg(true1),
				show_hardreg(dest1), show_hardreg(false1));
	zprintf(" end\n");
}

/* Select operations using a pointer condition. */

static void cg_select_ptr(struct hardreg* cond,
		struct hardreg* dest1, struct hardreg* dest2,
		struct hardreg* true1, struct hardreg* true2,
		struct hardreg* false1, struct hardreg* false2)
{
	zprintf("if %s then ", show_hardreg(cond));
	if (dest2)
		zprintf("%s = %s %s = %s else %s = %s %s = %s",
				show_hardreg(dest1), show_hardreg(true1),
				show_hardreg(dest2), show_hardreg(true2),
				show_hardreg(dest1), show_hardreg(false1),
				show_hardreg(dest2), show_hardreg(false2));
	else
		zprintf("%s = %s else %s = %s",
				show_hardreg(dest1), show_hardreg(true1),
				show_hardreg(dest1), show_hardreg(false1));
	zprintf(" end\n");
}

static void cg_call(struct hardreg* func,
		struct hardreg* dest1, struct hardreg* dest2)
{
	if (dest1)
		if (dest2)
			zprintf("%s, %s = %s(", show_hardreg(dest1), show_hardreg(dest2),
					show_hardreg(func));
		else
			zprintf("%s = %s(", show_hardreg(dest1), show_hardreg(func));
	else
		zprintf("%s(", show_hardreg(func));

	function_arg_list = 0;
}

static void cg_call_arg(struct hardreg* arg)
{
	if (function_arg_list > 0)
		zprintf(", ");
	zprintf("%s", show_hardreg(arg));
	function_arg_list++;
}

static void cg_call_end(void)
{
	zprintf(")\n");
}

/* Return. */

static void cg_ret(struct hardreg* reg1, struct hardreg* reg2)
{
	if (reg1)
	{
		if (reg2)
			zprintf("do return %s, %s end\n",
						show_hardreg(reg1),
						show_hardreg(reg2));
		else
			zprintf("do return %s end\n", show_hardreg(reg1));
	}
	else
		zprintf("do return end\n");
	zprintf("end\n");
}

/* Do a structure copy from one location to another. */

static void cg_memcpy(struct hardregref* src, struct hardregref* dest, int size)
{
	assert(src->type == TYPE_PTR);
	assert(dest->type == TYPE_PTR);

	zprintf("_memcpy(sp, stack, %s, %s, %s, %s, %d)\n",
			show_hardreg(dest->simple),
			show_hardreg(dest->base),
			show_hardreg(src->simple),
			show_hardreg(src->base),
			size);
}


const struct codegenerator cg_lua =
{
	.pointer_zero_offset = 1,
	.spname = "sp",
	.fpname = "fp",
	.stackname = "stack",

	.register_class =
	{
		[0] = REGTYPE_ALL
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

	.memcpyimpl = cg_memcpy
};
