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

static void init_sizes(void)
{
	max_alignment = 64;

	bits_in_bool = 1;
	bits_in_char = 64;
	bits_in_short = 64;
	bits_in_int = 64;
	bits_in_long = 64;
	bits_in_longlong = 64;

	max_int_alignment = 1;

	/*
	 * Floating point data types
	 */
	bits_in_float = 64;
	bits_in_double = 64;
	bits_in_longdouble = 64;

	max_fp_alignment = 1;

	/*
	 * Pointer data type
	 */
	bits_in_pointer = 64;
	pointer_alignment = 1;

	/*
	 * Enum data types
	 */
	bits_in_enum = 64;
	enum_alignment = 1;
}

static void emit_file_prologue(void)
{
	printf("require \"clue.crt\"\n");
	printf("local ptrstore = clue.crt.ptrstore\n");
	printf("local ptrload = clue.crt.ptrload\n");
	printf("local ptroffset = clue.crt.ptroffset\n");
	printf("local stackread = clue.crt.stackread\n");
	printf("local stackwrite = clue.crt.stackwrite\n");
	printf("local stackoffset = clue.crt.stackoffset\n");
	printf("local int = clue.crt.int\n");
	printf("local booland = clue.crt.booland\n");
	printf("local boolor = clue.crt.boolor\n");
	printf("local logand = clue.crt.logand\n");
	printf("local logor = clue.crt.logor\n");
	printf("local logxor = clue.crt.logxor\n");
	printf("local lognot = clue.crt.lognot\n");
	printf("local shl = clue.crt.shl\n");
	printf("local shr = clue.crt.shr\n");
	printf("local _memcpy = _memcpy\n");
}

int main(int argc, char **argv)
{
	init_sizes();
	init_register_allocator();

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

	if (die_if_error)
		return 1;
	return 0;
}
