/* main.c
 * Main program
 *
 * © 2008 David Given.
 * Clue is licensed under the Revised BSD open source license. To get the
 * full license text, see the README file.
 *
 * $Id: build 136 2008-03-22 19:00:08Z dtrg $
 * $HeadURL: https://primemover.svn.sf.net/svnroot/primemover/pm/lib/c.pm $
 * $LastChangedDate: 2007-04-30 22:41:42 +0000 (Mon, 30 Apr 2007) $
 */

#include "globals.h"
#include <stdarg.h>

const struct codegenerator* cg;

static void init_sizes(void)
{
	/* We say everything is 32 bits wide because that tells sparse how
	 * much precision an int can store for things like bitfields.
	 */
	bits_in_char = 32;
	max_alignment = bits_in_char;

	bits_in_bool = 1;
	bits_in_short = bits_in_char;
	bits_in_int = bits_in_char;
	bits_in_long = bits_in_char;
	bits_in_longlong = bits_in_char;

	max_int_alignment = 1;

	/*
	 * Floating point data types
	 */
	bits_in_float = bits_in_char;
	bits_in_double = bits_in_char;
	bits_in_longdouble = bits_in_char;

	max_fp_alignment = 1;

	/*
	 * Pointer data type; these are now stored unpacked, so they're twice
	 * as big.
	 */
	bits_in_pointer = bits_in_char*2;
	pointer_alignment = 1;

	/*
	 * Enum data types
	 */
	bits_in_enum = bits_in_char;
	enum_alignment = 1;
}

static void emit_file_prologue(void)
{
	zsetbuffer(ZBUFFER_HEADER);
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

int main(int argc, char **argv)
{
	init_sizes();
	init_register_allocator();

	cg = &cg_lua;

	struct string_list* filelist = NULL;
	struct symbol_list* symbols = sparse_initialize(argc, argv, &filelist);

	emit_file_prologue();
	compile_symbol_list(symbols);

	char *file;
	FOR_EACH_PTR_NOTAG(filelist, file)
	{
		symbols = sparse(file);
		compile_symbol_list(symbols);
	}
	END_FOR_EACH_PTR_NOTAG(file);

	zsetbuffer(ZBUFFER_HEADER);
	zflush(ZBUFFER_STDOUT);
	zprintf("\n");

	zsetbuffer(ZBUFFER_CODE);
	zflush(ZBUFFER_STDOUT);
	zprintf("\n");

	cg->function_prologue(NULL);
	cg->function_prologue_reg(&hardregs[0]);
	cg->function_prologue_reg(&hardregs[1]);
	cg->function_prologue_reg(&hardregs[2]);
	cg->function_prologue_end();

	zsetbuffer(ZBUFFER_INITIALIZER);
	zflush(ZBUFFER_STDOUT);

	cg->function_epilogue();

	if (die_if_error)
		return 1;
	return 0;
}
