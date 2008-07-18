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
	cg->prologue();
}

static struct
{
	const char* option;
	const struct codegenerator* cg;
}
generator_table[] = {
	{ "-mlua",     &cg_lua },
	{ "-mjs",      &cg_javascript }
};

static void init_code_generator(int* argc, const char* argv[])
{
	int i = 1;

	cg = NULL;
	while (argv[i])
	{
		int j;
		for (j=0; j<sizeof(generator_table)/sizeof(*generator_table); j++)
		{
			if (strcmp(argv[i], generator_table[j].option) == 0)
			{
				if (cg)
					die("you can only specify one backend at a time");
				cg = generator_table[j].cg;

				j = i;
				while (argv[j])
				{
					argv[j] = argv[j+1];
					j++;
				}

				(*argc)--;
			}
		}

		i++;
	}

	if (!cg)
		die("you must specify a code generator");
}

int main(int argc, const char* argv[])
{
	init_sizes();
	init_register_allocator();
	init_code_generator(&argc, argv);

	/* Poke some special lines into the parse buffer to set up the
	 * include paths.
	 */

	add_pre_buffer("#nostdinc\n");
	add_pre_buffer("#add_isystem \"src/libc/include\"\n");

	struct string_list* filelist = NULL;
	struct symbol_list* symbols = sparse_initialize(argc, (char**) argv, &filelist);

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

	emit_initializer();

	if (die_if_error)
		return 1;
	return 0;
}
