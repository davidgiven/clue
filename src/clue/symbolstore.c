/* symbolstore.c
 * Symbol auxiliary storage
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
#include "avl.h"

/* We frequently mangle the names of symbols in order to achieve various
 * kinds of uniqueness, and so need to keep track of what we've mangled
 * them to.
 */

static avltree_t symbolstore = NULL;

static int compare_cb(const void* lhs, const void* rhs)
{
	const struct sinfo* n1 = lhs;
	const struct sinfo* n2 = rhs;
	unsigned int i1 = (unsigned int) (size_t) n1->sym;
	unsigned int i2 = (unsigned int) (size_t) n2->sym;
	if (i1 < i2)
		return -1;
	else if (i1 > i2)
		return 1;
	return 0;
}

struct sinfo* lookup_sinfo_of_symbol(struct symbol* sym)
{
	struct sinfo key;
	key.sym = sym;

	struct sinfo* data = avl_search(symbolstore, compare_cb, &key, 0);
	if (!data)
	{
		data = calloc(sizeof(struct sinfo), 1);
		data->sym = sym;
		avl_insert(&symbolstore, compare_cb, data, NULL);

		const char* ident;
		if (sym->ident)
		{
			data->anonymous = 0;
			ident = show_ident(sym->ident);
		}
		else
		{
			data->anonymous = 1;
			data->here = 1;
			ident = "anon";
		}

		if (sym->ctype.modifiers & MOD_STATIC)
		{
			static int count = 0;

			data->here = 1;
			data->name = aprintf("static_%d_%d_%s", unique, count, ident);
			count++;
		}
		else
			data->name = aprintf("_%s", ident);
	}

	return data;
}

const char* show_symbol_mangled(struct symbol* sym)
{
	struct sinfo* node = lookup_sinfo_of_symbol(sym);
	return node->name;
}
