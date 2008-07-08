/* utils.c
 * Miscellaneous utilities
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

struct zprintnode
{
	struct zprintnode* next;
	char* value;
};

static struct zprintnode* zfirst = NULL;
static struct zprintnode* zlast = NULL;

const char* show_simple_pseudo(struct bb_state* state, pseudo_t pseudo)
{
	switch (pseudo->type)
	{
		case PSEUDO_VAL:
			return aprintf("%lld", pseudo->value);
			break;

		case PSEUDO_SYM:
			return show_symbol_mangled(pseudo->sym);
			if (pseudo->sym->ctype.modifiers & MOD_STATIC)
				return aprintf("_%s", show_pseudo(pseudo));
			/* fall through */

		case PSEUDO_ARG:
		{
			struct pinfo* pinfo = lookup_pinfo_of_pseudo(pseudo);
			assert(pinfo->stacked);

			return aprintf("stackread(sp, %d)", pinfo->stackoffset);
		}

		case PSEUDO_REG:
		{
			struct pinfo* pinfo = lookup_pinfo_of_pseudo(pseudo);

			if (pinfo->reg)
			{
				return aprintf("(pseudo %s is in %s)",
						show_pseudo(pseudo), show_hardreg(pinfo->reg));
			}

			return aprintf("(pseudo %s is not in a register?)", show_pseudo(pseudo));
		}

#if 0
		case PSEUDO_ARG:
		case PSEUDO_REG:
			def = pseudo->def;
			if (def && def->opcode == OP_SETVAL)
			{
				output_insn(state, "movl $<%s>,%s", show_pseudo(def->target),
						hardreg->name);
				break;
			}
			src = find_pseudo_storage(state, pseudo, hardreg);
			if (!src)
				break;
			if (src->flags & TAG_DEAD)
				mark_reg_dead(state, pseudo, hardreg);
			output_insn(state, "mov.%d %s,%s", 32, show_memop(src->storage),
					hardreg->name);
			break;
#endif

		default:
			return aprintf("(unknown simple pseudo type %d %s)",
					pseudo->type, show_pseudo(pseudo));
	}
}

const char* show_value(struct expression* expr)
{
	switch (expr->type)
	{
		case EXPR_VALUE:
			return aprintf("%lld", expr->value);

		case EXPR_FVALUE:
			return aprintf("%llf", expr->fvalue);

		default:
			return aprintf("(expression type %d)", expr->type);
	}
}

const char* aprintf(const char* fmt, ...)
{
	char* p;

	va_list ap;
	va_start(ap, fmt);
	vasprintf(&p, fmt, ap);
	va_end(ap);

	return p;
}

void zprintf(const char* format, ...)
{
	va_list ap;
	va_start(ap, format);
	zvprintf(format, ap);
	va_end(ap);
}

void zvprintf(const char* format, va_list ap)
{
	struct zprintnode* node = malloc(sizeof(struct zprintnode));
	node->next = NULL;
	vasprintf(&node->value, format, ap);

	if (zlast)
		zlast->next = node;
	zlast = node;

	if (!zfirst)
		zfirst = node;
}

void zflush(void)
{
	while (zfirst)
	{
		struct zprintnode* node = zfirst;
		zfirst = node->next;

		printf("%s", node->value);
		free(node->value);
		free(node);
	}

	zfirst = zlast = NULL;
}

