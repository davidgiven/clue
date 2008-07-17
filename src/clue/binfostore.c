/* binfostore.c
 * Basic block auxiliary storage
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

static avltree_t binfostore = NULL;

static int compare_cb(const void* lhs, const void* rhs)
{
	const struct binfo* n1 = lhs;
	const struct binfo* n2 = rhs;
	unsigned int i1 = (unsigned int) n1->bb;
	unsigned int i2 = (unsigned int) n2->bb;
	if (i1 < i2)
		return -1;
	else if (i1 > i2)
		return 1;
	return 0;
}

struct binfo* lookup_binfo_of_basic_block(struct basic_block* bb)
{
	struct binfo key;
	key.bb = bb;

	struct binfo* data = avl_search(binfostore, compare_cb, &key, 0);
	if (!data)
	{
		data = calloc(sizeof(struct binfo), 1);
		data->bb = bb;
		avl_insert(&binfostore, compare_cb, data, NULL);
	}

	return data;
}
