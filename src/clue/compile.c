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

static int emit_array_initializer(const char* luaname, int pos, struct expression* expr)
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
				E("ptrwrite(%s, %d, %d)\n", luaname, pos, c);
				pos++;
			}
			break;
		}

		case EXPR_INITIALIZER:
		{
			struct expression* e;

			FOR_EACH_PTR(expr->expr_list, e)
			{
				emit_array_initializer(luaname, 0, e);
			}
			END_FOR_EACH_PTR(e);
			break;
		}

		case EXPR_VALUE:
		{
			E("ptrwrite(%s, %d, %lld)\n", luaname, pos, expr->value);
			pos++;
			break;
		}

		case EXPR_FVALUE:
		{
			E("ptrwrite(%s, %d, %.15llg)\n", luaname, pos, expr->fvalue);
			pos++;
			break;
		}

		case EXPR_POS:
		{
			pos = expr->init_offset;
			emit_array_initializer(luaname, pos, expr->init_expr);
			break;
		}

		case EXPR_SYMBOL:
		{
			declare_symbol(expr->symbol);
			E("ptrwrite(%s, %d, %s)\n", luaname, pos, show_symbol_mangled(expr->symbol));
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

					declare_symbol(sym);
					E("_memcpy(nil, nil, %s, %s, %d)\n",
							luaname,
							show_symbol_mangled(sym),
							bits_to_bytes(sym->bit_size));
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

static void emit_array(struct symbol *sym)
{
	const char* luaname = show_symbol_mangled(sym);
	if (sym->initializer)
		emit_array_initializer(luaname, 0, sym->initializer);
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
	const char* name = sinfo->name;

	if (sinfo->declared)
		return;
    sinfo->declared = 1;

    if (!sinfo->here)
    {
    	/* If extern, import the symbol. */

    	E("local %s = _G.%s\n", name, name);
    }
    else
    {
    	/* Otherwise, define storage for the symbol (but don't initialise
    	 * it yet).
    	 */
    	if (type->type != SYM_FN)
    		E("local %s = newptr()\n", name);

		/* Anonymous objects get defined immediately. */

		if (sinfo->anonymous)
			define_symbol(sym);
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
		case SYM_BASETYPE:
		case SYM_PTR:
			emit_array(sym);
			break;

		case SYM_FN:
		{
			expand_symbol(sym);
			struct entrypoint *ep = linearize_symbol(sym);
			if (ep)
			{
			    compile_references_for_function(ep);
			    zflush();
				generate_ep(ep);
				if (!(sym->ctype.modifiers & MOD_STATIC))
					E("_G.%s = %s\n", name, name);
				zflush();
			}
			break;
		}

		default:
			printf("define symbol %s type %d\n", name, type->type);
	}
}

/* Create storage for a symbol. This also lets us take account of all symbols
 * defined in this file, so we can distinguish them from imported ones. */

static void create_storage_for_symbol(struct symbol* sym)
{
	struct symbol* type = sym->ctype.base_type;
	if (!type)
		return;

	struct sinfo* sinfo = lookup_sinfo_of_symbol(sym);
	const char* name = sinfo->name;

	if (sinfo->created)
		return;

	sinfo->here = 1;
    sinfo->declared = 1;
    sinfo->created = 1;

    if (type->type == SYM_FN)
    {
    	E("local %s\n", name);
    }
    else
    {
    	E("local %s = newptr()\n", name);
		if (!(sym->ctype.modifiers & MOD_STATIC))
			E("_G.%s = %s\n", name, name);
    }
}

/* Compile all symbols in a list. */

int compile_symbol_list(struct symbol_list *list)
{
	struct symbol *sym;

	/* Expand all symbols. */

	FOR_EACH_PTR(list, sym)
	{
		expand_symbol(sym);
	}
	END_FOR_EACH_PTR(sym);

	/* First, create storage for all defined symbols. */

	FOR_EACH_PTR(list, sym)
	{
		create_storage_for_symbol(sym);
	}
	END_FOR_EACH_PTR(sym);

	/* Now, define all functions. */

	FOR_EACH_PTR(list, sym)
	{
		struct symbol* type = sym->ctype.base_type;
		if (type && (type->type == SYM_FN))
		{
			define_symbol(sym);
			zflush();
		}
	}
	END_FOR_EACH_PTR(sym);

	/* Now declare and compile everything else. This is done last to allow
	 * initialisers to refer to function objects, which can't be forward
	 * declared. */

	FOR_EACH_PTR(list, sym)
	{
		expand_symbol(sym);
		struct symbol* type = sym->ctype.base_type;
		if (type && (type->type != SYM_FN))
		{
			define_symbol(sym);
			zflush();
		}
	}
	END_FOR_EACH_PTR(sym);

	return 0;
}

