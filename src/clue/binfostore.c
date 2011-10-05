/* binfostore.c
 * Basic block auxiliary storage
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

static avltree_t binfostore = NULL;
static struct binfo** binfolist = NULL;
static int binfocount = 0;
static int iterator;

static int compare_cb(const void* lhs, const void* rhs)
{
	const struct binfo* n1 = lhs;
	const struct binfo* n2 = rhs;
	unsigned int i1 = (unsigned int) (size_t)n1->bb;
	unsigned int i2 = (unsigned int) (size_t)n2->bb;
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

void reset_binfo(void)
{
	binfostore = NULL;
	binfolist = NULL;
}

static void enumerate_cb(void* node)
{
	binfocount++;
}

static void iterate_cb(void* node)
{
	binfolist[iterator] = node;
	iterator++;
}

static int sortindex_cb(const void* p1, const void* p2)
{
	struct binfo* const * o1 = p1;
	struct binfo* const * o2 = p2;
	int id1 = (*o1)->id;
	int id2 = (*o2)->id;

	if (id1 < id2)
		return -1;
	else if (id1 > id2)
		return 1;
	return 0;
}

void get_binfo_list(struct binfo*** list, int* count)
{
	if (!binfolist)
	{
		binfocount = 0;
		avl_traverse(binfostore, enumerate_cb);

		binfolist = calloc(sizeof(struct binfo*), binfocount);
		iterator = 0;
		avl_traverse(binfostore, iterate_cb);

		qsort(binfolist, binfocount, sizeof(struct binfo*),
				sortindex_cb);
	}

	*list = binfolist;
	*count = binfocount;
}

