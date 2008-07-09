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

