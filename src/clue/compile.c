/* compile.c
 * Symbol emission
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

static void declare_symbol(struct symbol* sym);
static void pass1_define_symbol(struct symbol* sym);

static struct hardreg* target_ptr;
static struct symbol_list* symbols_to_initialize = NULL;

/* Emits elements from within an array initializer. */

static int emit_array_initializer(int pos, struct expression* expr)
{
	switch (expr->type)
	{
		case EXPR_STRING:
		{
			struct hardreg* reg = allocate_hardreg(REGTYPE_INT);

			struct string* s = expr->string;
			int i = 0;
			for (i = 0; i < s->length; i++)
			{
				int c = s->data[i];
				cg->set_int(c, reg);
				cg->store(NULL, target_ptr, pos, reg);
				pos++;
			}

			unref_hardreg(reg);
			break;
		}

		case EXPR_INITIALIZER:
		{
			struct expression* e;

			FOR_EACH_PTR(expr->expr_list, e)
			{
				emit_array_initializer(pos, e);
			}
			END_FOR_EACH_PTR(e);
			break;
		}

		case EXPR_VALUE:
		{
			struct hardreg* reg = allocate_hardreg(REGTYPE_INT);

			cg->set_int(expr->value, reg);
			cg->store(NULL, target_ptr, pos, reg);
			pos++;

			unref_hardreg(reg);
			break;
		}

		case EXPR_FVALUE:
		{
			struct hardreg* reg = allocate_hardreg(REGTYPE_FLOAT);

			cg->set_float(expr->fvalue, reg);
			cg->store(NULL, target_ptr, pos, reg);
			pos++;

			unref_hardreg(reg);
			break;
		}

		case EXPR_POS:
		{
			pos = expr->init_offset + cg->pointer_zero_offset;
			emit_array_initializer(pos, expr->init_expr);
			break;
		}

		case EXPR_SYMBOL:
		{
			struct hardreg* reg = allocate_hardreg(REGTYPE_OPTR);

			declare_symbol(expr->symbol);
			cg->set_symbol(expr->symbol, reg);
			cg->store(NULL, target_ptr, pos, reg);
			pos++;

			unref_hardreg(reg);
			break;
		}

		case EXPR_PREOP:
		{
			switch (expr->op)
			{
				case '*': /* memory dereference --- i.e., structure copy */
				{
					assert(expr->unop->type == EXPR_SYMBOL);
					struct symbol* sym = expr->unop->symbol;

					assert(sym->initializer);
					emit_array_initializer(pos, sym->initializer);
					break;
				}

				default:
					printf("unknown structure opcode %c\n", expr->op);
					assert(0);
			}
			break;
		}

		default:
			printf("unable to initialize:\n");
			show_expression(expr);
			assert(0);
	}

	return pos;
}

/* Emit code to define an uninitialized global array. */

static void emit_array(struct symbol* sym)
{
	if (sym->initializer)
	{
		cg->set_symbol(sym, target_ptr);

		emit_array_initializer(cg->pointer_zero_offset, sym->initializer);
	}
}

/* Emit code to define an initialized scalar. */

static void emit_scalar(struct symbol* sym)
{
	struct expression* expr = sym->initializer;
	if (!expr)
		return;

	cg->set_symbol(sym, target_ptr);
	switch (expr->type)
	{
		case EXPR_SYMBOL:
		{
			struct hardreg* reg = allocate_hardreg(REGTYPE_OPTR);

			declare_symbol(expr->symbol);
			cg->set_int(cg->pointer_zero_offset, reg);
			cg->store(NULL, target_ptr, cg->pointer_zero_offset, reg);

			unref_hardreg(reg);
			reg = allocate_hardreg(REGTYPE_INT);

			cg->set_symbol(expr->symbol, reg);
			cg->store(NULL, target_ptr, cg->pointer_zero_offset, reg);

			unref_hardreg(reg);
			break;
		}

		default:
			assert(0);
	}
}

/* Compile all referenced symbols for a function. */

static void compile_references_for_function(struct entrypoint* ep)
{
	pseudo_t pseudo;
	FOR_EACH_PTR(ep->accesses, pseudo)
	{
		if (pseudo->type == PSEUDO_SYM)
		{
			struct symbol* sym = pseudo->sym;

			if ((sym->ctype.modifiers & MOD_STATIC) ||
					(sym->ctype.modifiers & MOD_EXTERN))
			{
				declare_symbol(sym);
			}
		}
	}
	END_FOR_EACH_PTR(pseudo);
}

/* Declare a single symbol. */

static void declare_symbol(struct symbol* sym)
{
	struct sinfo* sinfo = lookup_sinfo_of_symbol(sym);

	if (sinfo->declared)
		return;
	sinfo->declared = 1;

	zsetbuffer(ZBUFFER_HEADER);
	cg->comment("symbol %s (%p), here=%d, static=%d\n",
			show_symbol_mangled(sym), sym, sinfo->here, !!(sym->ctype.modifiers
					& MOD_STATIC));
	cg->declare(sym);

	/* ...and queue the pass 2 declaration. */

	add_symbol(&symbols_to_initialize, sym);
}

/* Compile a single symbol in pass1. If it's a function, define immediately;
 * otherwise, add it to the list of things to define later. */

static void pass1_define_symbol(struct symbol* sym)
{
	struct symbol* type = sym->ctype.base_type;
	struct sinfo* sinfo = lookup_sinfo_of_symbol(sym);

	if (sinfo->defined)
		return;

	switch (type->type)
	{
		case SYM_FN:
		{
			struct entrypoint *ep = linearize_symbol(sym);
			if (ep)
			{
				compile_references_for_function(ep);
				generate_ep(ep);
				zsetbuffer(ZBUFFER_FUNCTION);
				zflush(ZBUFFER_CODE);
			}
			break;
		}
	}
}

/* Compile a single symbol during pass 2. */

static void pass2_define_or_declare_symbol(struct symbol* sym)
{
	struct symbol* type = sym->ctype.base_type;
	struct sinfo* sinfo = lookup_sinfo_of_symbol(sym);
	const char* name = sinfo->name;

	if (sinfo->defined)
		return;
	sinfo->defined = 1;

	if (!sinfo->here)
	{
		/* If extern, import the symbol. */

		cg->import(sym);
	}
	else
	{
		/* Otherwise, define storage for the symbol. */

		if (type->type != SYM_FN)
			cg->create_storage(sym);

		/* Export the symbol if necessary. */

		if (!(sym->ctype.modifiers & MOD_STATIC))
			cg->export(sym);

		/* And initialize. */

		switch (type->type)
		{
			case SYM_ARRAY:
			case SYM_STRUCT:
				emit_array(sym);
				break;

			case SYM_BASETYPE:
			case SYM_PTR:
				emit_scalar(sym);
				break;

			case SYM_FN:
				break;

			default:
				printf("define symbol %s type %d\n", name, type->type);
		}
	}
}

/* Compile all symbols in a list. */

int compile_symbol_list(struct symbol_list *list)
{
	struct symbol *sym;

	/* For each symbol: */

	FOR_EACH_PTR(list, sym)
	{
		/* Expand it... */

		expand_symbol(sym);

		/* Mark it as coming from here... */

		struct sinfo* sinfo = lookup_sinfo_of_symbol(sym);
		sinfo->here = 1;

		/* Declare it... */

		declare_symbol(sym);

		/* ...do the pass 1 definition... */

		pass1_define_symbol(sym);
		break;
	}
	END_FOR_EACH_PTR(sym);

#if 0
	/* Now, declare and define all symbols. */

	FOR_EACH_PTR(list, sym)
	{
		declare_symbol(sym);
		define_symbol(sym);
	}
	END_FOR_EACH_PTR(sym);
#endif

	return 0;
}

/* Emit a function that will initialize all of this file's global data. */

void emit_initializer(void)
{
	reset_hardregs();
	untouch_hardregs();

	cg->function_prologue(NULL);

	/* Allocate an object pointer register to hold the thing we're
	 * initializing. */

	target_ptr = allocate_hardreg(REGTYPE_OPTR);

	/* Allocate and declare one register of each class. This ensures
	 * that there's an available register later when we emit the
	 * initializer code.
	 */

	int i;
	for (i = 0; i < NUM_REG_CLASSES; i++)
	{
		struct hardreg* reg = allocate_hardreg(1 << i);
		unref_hardreg(reg);
	}

	for (i = 0; i < NUM_REGS; i++)
	{
		struct hardreg* reg = &hardregs[i];
		if (reg->touched)
			cg->function_prologue_reg(reg);
	}

	cg->function_prologue_end();

	zsetbuffer(ZBUFFER_INITIALIZER);
	zflush(ZBUFFER_STDOUT);

	struct symbol* sym;
	FOR_EACH_PTR(symbols_to_initialize, sym)
	{
		pass2_define_or_declare_symbol(sym);
	}
	END_FOR_EACH_PTR(sym);

	cg->return_void();
	cg->function_epilogue();
}
