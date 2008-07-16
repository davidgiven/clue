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
static void define_symbol(struct symbol* sym);

/* Emits elements from within an array initializer. */

static int emit_array_initializer(int pos, struct expression* expr)
{
	switch (expr->type)
	{
		case EXPR_STRING:
		{
			struct string* s = expr->string;
			int i = 0;
			for (i = 0; i < s->length; i++)
			{
				int c = s->data[i];
				cg->set_int(c, &hardregs[1]);
				cg->store(NULL, &hardregs[0], pos, &hardregs[1]);
				pos++;
			}
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
			cg->set_int(expr->value, &hardregs[1]);
			cg->store(NULL, &hardregs[0], pos, &hardregs[1]);
			pos++;
			break;
		}

		case EXPR_FVALUE:
		{
			cg->set_float(expr->fvalue, &hardregs[1]);
			cg->store(NULL, &hardregs[0], pos, &hardregs[1]);
			pos++;
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
			declare_symbol(expr->symbol);
			cg->set_symbol(expr->symbol, &hardregs[1]);
			cg->store(NULL, &hardregs[0], pos, &hardregs[1]);
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
		cg->set_symbol(sym, &hardregs[0]);
		emit_array_initializer(cg->pointer_zero_offset, sym->initializer);
	}
}

/* Emit code to define an initialized scalar. */

static void emit_scalar(struct symbol* sym)
{
	struct expression* expr = sym->initializer;
	if (!expr)
		return;

	cg->set_symbol(sym, &hardregs[0]);
	switch (expr->type)
	{
		case EXPR_SYMBOL:
		{
			declare_symbol(expr->symbol);
			cg->set_int(cg->pointer_zero_offset, &hardregs[1]);
			cg->store(NULL, &hardregs[0], cg->pointer_zero_offset, &hardregs[1]);
			cg->set_symbol(expr->symbol, &hardregs[1]);
			cg->store(NULL, &hardregs[0], cg->pointer_zero_offset+1, &hardregs[1]);
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
	struct symbol* type = sym->ctype.base_type;
	struct sinfo* sinfo = lookup_sinfo_of_symbol(sym);

	if (sinfo->declared)
		return;
    sinfo->declared = 1;

    zsetbuffer(ZBUFFER_HEADER);
    cg->comment("symbol %s (%p), here=%d, static=%d\n",
    		show_symbol_mangled(sym), sym,
    		sinfo->here, !!(sym->ctype.modifiers & MOD_STATIC));
    cg->declare(sym);

	zsetbuffer(ZBUFFER_INITIALIZER);
    if (!sinfo->here)
    {
    	/* If extern, import the symbol. */

    	cg->import(sym);
    }
    else
    {
    	/* Otherwise, define storage for the symbol (but don't initialise
    	 * it yet).
    	 */
    	if (type->type != SYM_FN)
    		cg->create_storage(sym);

		/* We need to manually define anonymous symbols because they're not
		 * in the symbol list. */

		if (sinfo->anonymous)
			define_symbol(sym);

	    /* Export the symbol if necessary. */

		if (!(sym->ctype.modifiers & MOD_STATIC))
			cg->export(sym);
    }
}

/* Compile a single symbol (either generating code or an initializer). */

static void define_symbol(struct symbol* sym)
{
	struct symbol* type = sym->ctype.base_type;
	struct sinfo* sinfo = lookup_sinfo_of_symbol(sym);
	const char* name = sinfo->name;

	if (sinfo->defined)
		return;
	sinfo->defined = 1;

	switch (type->type)
	{
		case SYM_ARRAY:
		case SYM_STRUCT:
			zsetbuffer(ZBUFFER_INITIALIZER);
			emit_array(sym);
			break;

		case SYM_BASETYPE:
		case SYM_PTR:
			zsetbuffer(ZBUFFER_INITIALIZER);
			emit_scalar(sym);
			break;

		case SYM_FN:
		{
			expand_symbol(sym);
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

		default:
			printf("define symbol %s type %d\n", name, type->type);
	}
}

/* Compile all symbols in a list. */

int compile_symbol_list(struct symbol_list *list)
{
	struct symbol *sym;

	/* Expand all symbols, and mark all defined symbols as coming from here. */

	FOR_EACH_PTR(list, sym)
	{
		expand_symbol(sym);

		struct sinfo* sinfo = lookup_sinfo_of_symbol(sym);
		sinfo->here = 1;
	}
	END_FOR_EACH_PTR(sym);

	/* Now, declare and define all symbols. */

	FOR_EACH_PTR(list, sym)
	{
		declare_symbol(sym);
		define_symbol(sym);
	}
	END_FOR_EACH_PTR(sym);

	return 0;
}

