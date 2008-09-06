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

struct zbuffer
{
	struct zprintnode* zfirst;
	struct zprintnode* zlast;
};

static struct zbuffer zbuffers[ZBUFFER__MAX];
static struct zbuffer* currentbuffer = &zbuffers[ZBUFFER_CODE];

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
	if (!currentbuffer)
		vprintf(format, ap);
	else
	{
		struct zprintnode* node = malloc(sizeof(struct zprintnode));
		node->next = NULL;
		vasprintf(&node->value, format, ap);

		if (currentbuffer->zlast)
			currentbuffer->zlast->next = node;
		currentbuffer->zlast = node;

		if (!currentbuffer->zfirst)
			currentbuffer->zfirst = node;
	}
}

void zflush(int buffer)
{
	struct zbuffer* buf = currentbuffer;
	zsetbuffer(buffer);

	while (buf->zfirst)
	{
		struct zprintnode* node = buf->zfirst;
		buf->zfirst = node->next;

		zprintf("%s", node->value);

		free(node->value);
		free(node);
	}

	buf->zfirst = buf->zlast = NULL;
}

void zsetbuffer(int buffer)
{
	if (buffer == ZBUFFER_STDOUT)
		currentbuffer = NULL;
	else
	{
		assert(buffer >= 0);
		assert(buffer < ZBUFFER__MAX);

		currentbuffer = &zbuffers[buffer];
	}
}

void dump_bb(struct basic_block *bb, unsigned generation)
{
	struct instruction *insn;

	bb->generation = generation;

	printf(".L%p\n", bb);

	FOR_EACH_PTR(bb->insns, insn) {
		if (!insn->bb)
			continue;
		printf("\t%s\n", show_instruction(insn));
	}
	END_FOR_EACH_PTR(insn);

	printf("\n");
}

void dump_fn(struct entrypoint *ep)
{
	struct basic_block *bb;
	unsigned long generation = ++bb_generation;
	struct symbol *sym = ep->name;
	const char *name = show_ident(sym->ident);

	if (sym->ctype.modifiers & MOD_STATIC)
		printf("\n\n%s:\n", name);
	else
		printf("\n\n.globl %s\n%s:\n", name, name);

	unssa(ep);

	FOR_EACH_PTR(ep->bbs, bb) {
		if (bb->generation == generation)
			continue;
		dump_bb(bb, generation);
	}
	END_FOR_EACH_PTR(bb);
}

