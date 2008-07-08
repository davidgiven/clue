/* pinfostore.c
 * Pseudo auxiliary storage
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
#include "avl.h"

static avltree_t pinfostore = NULL;

static int compare_cb(const void* lhs, const void* rhs)
{
	const struct pinfo* n1 = lhs;
	const struct pinfo* n2 = rhs;
	unsigned int i1 = (unsigned int) n1->pseudo;
	unsigned int i2 = (unsigned int) n2->pseudo;
	if (i1 < i2)
		return -1;
	else if (i1 > i2)
		return 1;
	return 0;
}

static void reset_cb(void* user)
{
	struct pinfo* pinfo = user;
	pinfo->reg = NULL;
}

void reset_pinfo(void)
{
	avl_traverse(pinfostore, reset_cb);
}

struct pinfo* lookup_pinfo_of_pseudo(pseudo_t pseudo)
{
	struct pinfo key;
	key.pseudo = pseudo;

	struct pinfo* pinfo = avl_search(pinfostore, compare_cb, &key, 0);
	if (pinfo)
		return pinfo;

	pinfo = malloc(sizeof(struct pinfo));
	memset(pinfo, 0, sizeof(struct pinfo));
	pinfo->pseudo = pseudo;

	avl_insert(&pinfostore, compare_cb, pinfo, NULL);
	return pinfo;
}
