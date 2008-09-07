/* avl.c
 * Generalised AVL tree routine
 *
 * © 2008 David Given.
 * Clue is licensed under the Revised BSD open source license. To get the
 * full license text, see the README file.
 *
 * $Id$
 * $HeadURL$
 * $LastChangedDate: 2007-04-30 22:41:42 +0000 (Mon, 30 Apr 2007) $
 */

#include <stdlib.h>
#include <stdio.h>
#include "avl.h"

/* --- Useful macros & definitions --------------------------------------- */

#define MIN(a, b) ((a)<(b) ? (a) : (b))
#define MAX(a, b) ((a)>(b) ? (a) : (b))

#define LEFT (0)
#define RIGHT (1)
#define OPPOSITE(d) (1-(d))

#define LEFT_HEAVY (-1)
#define BALANCED (0)
#define RIGHT_HEAVY (1)

#define HEIGHT_NOCHANGE 0
#define HEIGHT_CHANGE 1

#define LEFT_IMBALANCE(bal) (bal < LEFT_HEAVY)
#define RIGHT_IMBALANCE(bal) (bal > RIGHT_HEAVY)

#define EQ_CMP 0
#define MIN_CMP -1
#define MAX_CMP 1

/* --- Debug code -------------------------------------------------------- */

#ifdef DEBUG
#define ASSERT(n, m) assert(n)
#else
#define ASSERT(n, m) do {} while(0)
#endif

#define CHECKMAGIC(n) \
	ASSERT(n->magic == AVLTREE_MAGIC, "Bad juju in AVL node!")

/* --- Constructors and Destructors -------------------------------------- */

static struct avlnode* new_avlnode(void* data)
{
	struct avlnode* a = (struct avlnode*)malloc(sizeof(struct avlnode));
#ifdef DEBUG
	a->magic = AVLTREE_MAGIC;
#endif
	a->data = data;
	a->subtree[LEFT] = NULL;
	a->subtree[RIGHT] = NULL;
	a->balance = 0;
	return a;
}

static void delete_avlnode(struct avlnode* node)
{
	CHECKMAGIC(node);
	if (node->subtree[LEFT])
		delete_avlnode(node->subtree[LEFT]);
	if (node->subtree[RIGHT])
		delete_avlnode(node->subtree[RIGHT]);
#ifdef DEBUG
	node->magic = 0;
#endif
	free(node);
}

/* --- Rotating and rebalancing ------------------------------------------ */

static int rotate_once(struct avlnode** rootp, int dir)
{
	int otherdir = OPPOSITE(dir);
	struct avlnode* root = *rootp;
	struct avlnode* oldroot = root;

	/* If otherdir subtree is balanced, this rotation will not change
	 * the overall tree height. Otherwise, the rotation will shorten
	 * the tree. */

	int heightchange = (root->subtree[otherdir]->balance == 0)
			  ? HEIGHT_NOCHANGE
			  : HEIGHT_CHANGE;

	/* Assign new root */

	CHECKMAGIC(root);
	root = *rootp = oldroot->subtree[otherdir];
	CHECKMAGIC(root);

	/* new-root exchanges its "dir" subtree for its parent */

	oldroot->subtree[otherdir] = root->subtree[dir];
	root->subtree[dir] = oldroot;

	/* update balances */

	if (dir == LEFT)
		root->balance--;
	else
		root->balance++;
	oldroot->balance = -root->balance;

	return heightchange;
}

static int rotate_twice(struct avlnode** rootp, int dir)
{
	int otherdir = OPPOSITE(dir);
	struct avlnode* root = *rootp;
	struct avlnode* oldroot = root;
	struct avlnode* oldotherdirsubtree = root->subtree[otherdir];

	/* Assign new root */

	CHECKMAGIC(oldroot);
	root = *rootp = oldroot->subtree[otherdir]->subtree[dir];
	CHECKMAGIC(root);

	/* New-root exchanges its "dir" subtree for its grandparent */

	oldroot->subtree[otherdir] = root->subtree[dir];
	root->subtree[dir] = oldroot;

	/* New-root exchanges its "other-dir" subtree for its parent */

	oldotherdirsubtree->subtree[dir] = root->subtree[otherdir];
	root->subtree[otherdir] = oldotherdirsubtree;

	/* Update balances */

	root->subtree[LEFT]->balance  = -MAX(root->balance, 0);
	root->subtree[RIGHT]->balance = -MIN(root->balance, 0);
	root->balance = 0;

	/* A double rotation always shortens the overall height of the tree */

	return HEIGHT_CHANGE;
}

static int rebalance(struct avlnode** rootp)
{
	struct avlnode* root = *rootp;
	int heightchange = HEIGHT_NOCHANGE;

	CHECKMAGIC(root);
	if (LEFT_IMBALANCE(root->balance))
	{
		/* Needs a right rotation */
		CHECKMAGIC(root->subtree[LEFT]);
		if (root->subtree[LEFT]->balance == RIGHT_HEAVY)
		{
			/* RL rotation needed */
			heightchange = rotate_twice(rootp, RIGHT);
		}
		else
		{
			/* RR rotation needed */
			heightchange = rotate_once(rootp, RIGHT);
		}
	}
	else if (RIGHT_IMBALANCE(root->balance))
	{
		/* Needs a left rotation */

		CHECKMAGIC(root->subtree[RIGHT]);
		if (root->subtree[RIGHT]->balance == LEFT_HEAVY)
		{
			/* LR rotation needed */
			heightchange = rotate_twice(rootp, LEFT);
		}
		else
		{
			/* LL rotation needed */
			heightchange = rotate_once(rootp, LEFT);
		}
	}

	return  heightchange;
}

/* --- Comparisons ------------------------------------------------------- */

static int compare(struct avlnode* this, avl_comparison_cb_t comparefunc,
	void* target, int cmp)
{
	CHECKMAGIC(this);
	switch (cmp)
	{
		case EQ_CMP: /* Standard comparison */
			return comparefunc(this->data, target);

		case MIN_CMP: /* Find minimal item */
			return (!this->subtree[LEFT] ? 0 : -1);

		case MAX_CMP: /* Find maximal item */
			return (!this->subtree[RIGHT] ? 0 : 1);
	}
	ASSERT(1==0, "Reached unreachable code");
	return -999; /* Should never get here */
}

/* --- Search/insert/delete ---------------------------------------------- */

void* avl_search(avltree_t root, avl_comparison_cb_t comparefunc,
	void* target, int cmp)
{
	int result;

	ASSERT(avl_validate(root, comparefunc), "Tree not valid");
	while (root && ((result = compare(root, comparefunc, target, cmp)) != 0))
		root = root->subtree[(result<0) ? LEFT : RIGHT];

	return root ? root->data : NULL;
}

void* avl_insert(struct avlnode** rootp, avl_comparison_cb_t comparefunc,
	void* item, int* change)
{
	struct avlnode* root = *rootp;
	void* found = NULL;
	int increase = 0;
	int result;
	int dir;
	int junk = 0;

	if (!change)
		change = &junk;

	/* See if the tree is empty */

	if (root == NULL)
	{
		*rootp = new_avlnode(item);
		*change = HEIGHT_CHANGE;
		return NULL;
	}

//	debug_printobjdict(root);
	ASSERT(avl_validate(root, comparefunc), "Tree not valid");
//	printf("\n");

	/* Compare items and determine which direction to search */

	result = compare(root, comparefunc, item, EQ_CMP);
	dir = (result == -1) ? LEFT : RIGHT;

	if (result != 0)
	{
		/* Insert into "dir" subtree */

		found = avl_insert(&root->subtree[dir], comparefunc, item, change);
		if (found)
			return found; /* Already here --- don't insert */
		/* Set balance factor increment */

		increase = result * (*change);

//		msg(MSG_DEBUG, "Inserted into %x, increase=%d", root, increase);
	}
	else
	{
		/* Key already in tree at this node */

		increase = HEIGHT_NOCHANGE;
		return root->data;
	}

	/* Update balance factor */

	root->balance += increase;

	/* Rebalance if needed. The height increases only if its subtree
	 * height increases and the current tree needs no rotation. */

	*change = (increase && root->balance)
		  ? (1 - rebalance(rootp))
		  : HEIGHT_NOCHANGE;
	return NULL;
}

void* avl_delete(struct avlnode** rootp, avl_comparison_cb_t comparefunc,
	void* item, int* change, int cmp)
{
	struct avlnode* root = *rootp;
	void* found = NULL;
	int decrease = 0;
	int result;
	int dir;
	int junk = 0;

	if (!change)
		change = &junk;

	/* See if the tree is empty */

	if (!root)
	{
		/* Key not found */

		*change = HEIGHT_NOCHANGE;
		return NULL;
	}

	ASSERT(avl_validate(root, comparefunc), "Tree not valid");
	/* Compare items and determine which direction to search */

	result = compare(root, comparefunc, item, cmp);
	dir = (result == -1) ? LEFT : RIGHT;

	if (result != 0)
	{
		/* Delete from "dir" subtree */

		found = avl_delete(&root->subtree[dir], comparefunc, item, change, cmp);
		if (!found)
			return NULL; /* Not found --- can't delete */

		/* Set balance factor decrement */

		decrease = result * (*change);
	}
	else
	{
		/* Found key at this node */

		found = root->data;

		/* At this point we know "result" is zero and "root" points to
		 * the node that we need to delete.  There are three cases:
		 *
		 *    1) The node is a leaf.  Remove it and return.
		 *
		 *    2) The node is a branch (has only 1 child). Make "root"
		 *       (the pointer to this node) point to the child.
		 *
		 *    3) The node has two children. We swap items with the successor
		 *       of "root" (the smallest item in its right subtree) and delete
		 *       the successor from the right subtree of "root".  The
		 *       identifier "decrease" should be reset if the subtree height
		 *       decreased due to the deletion of the successor of "root".
		 */

		if ((root->subtree[LEFT] == NULL) &&
		    (root->subtree[RIGHT] == NULL))
		{
			/* We have a leaf --- remove it. */

			delete_avlnode(root);
			*rootp = NULL;
			*change = HEIGHT_CHANGE;
			return found;
		}
		else if ((root->subtree[LEFT] == NULL) ||
			 (root->subtree[RIGHT] == NULL))
		{
			/* We have one child --- only child becomes new root */

			struct avlnode* todelete = root;
			if (root->subtree[RIGHT])
				*rootp = root->subtree[RIGHT];
			else
				*rootp = root->subtree[LEFT];

			/* We just shortened the subtree. */

			*change = HEIGHT_CHANGE;

			/* Null-out the subtree pointers so we don't recursively
			 * delete. */

			todelete->subtree[LEFT] = NULL;
			todelete->subtree[RIGHT] = NULL;

			delete_avlnode(todelete);
			return found;
		}
		else
		{
			/* We have two children. We find the successor and replace our
			 * current data item with that of the successor. */

			 root->data = avl_delete(&root->subtree[RIGHT], comparefunc, item, &decrease, MIN_CMP);
		}
	}

	/* Update balance factor */

	root->balance -= decrease;

	/* Rebalance if necessary -- the height of current tree changes if one
	 * of two things happens: (1) a rotation was performed which changed
	 * the height of the subtree (2) the subtree height decreased and now
	 * matches the height of its other subtree (so the current tree now
	 * has a zero balance when it previously did not). */

	if (decrease)
	{
		if (root->balance)
			*change = rebalance(rootp);
		else
			*change = HEIGHT_CHANGE;
	}
	else
		change = HEIGHT_NOCHANGE;

	return  found;
}

void avl_traverse(struct avlnode* node, avl_item_cb_t callbackfunc)
{
	if (node == NULL)
		return;

	callbackfunc(node->data);

	avl_traverse(node->subtree[LEFT], callbackfunc);
	avl_traverse(node->subtree[RIGHT], callbackfunc);
}

void avl_nuke(struct avlnode* node, avl_item_cb_t deletefunc)
{
	if (node == NULL)
		return;

	deletefunc(node->data);

	avl_nuke(node->subtree[LEFT], deletefunc);
	avl_nuke(node->subtree[RIGHT], deletefunc);
	node->subtree[LEFT] = NULL;
	node->subtree[RIGHT] = NULL;
	delete_avlnode(node);
}

/* --- Verification ------------------------------------------------------ */

static int height(struct avlnode* this)
{
	int leftheight  = (this->subtree[LEFT])  ? height(this->subtree[LEFT])  : 0;
	int rightheight = (this->subtree[RIGHT]) ? height(this->subtree[RIGHT]) : 0;
	return (1 + MAX(leftheight, rightheight));
}

int avl_validate(struct avlnode* this, avl_comparison_cb_t comparefunc)
{
	int valid = 1;
	int leftheight;
	int rightheight;
	int diffheight;

	/* First verify that subtrees are correct */

	if (!this)
		return 1;
	CHECKMAGIC(this);
	if (this->subtree[LEFT])
		valid *= avl_validate(this->subtree[LEFT], comparefunc);
	if (this->subtree[RIGHT])
		valid *= avl_validate(this->subtree[RIGHT], comparefunc);

	/* Now get the height of each subtree */

	leftheight  = (this->subtree[LEFT])  ? height(this->subtree[LEFT])  : 0;
	rightheight = (this->subtree[RIGHT]) ? height(this->subtree[RIGHT]) : 0;

	/* Verify that AVL tree property is satisfied */

	diffheight = rightheight - leftheight;
	if (LEFT_IMBALANCE(diffheight) || RIGHT_IMBALANCE(diffheight))
	{
		valid = 0;
		fprintf(stderr, "AVL tree failure: height difference is %d at node %p\n", diffheight, this->data);
	}

	/* Verify that balance-factor is correct */

	if (diffheight != this->balance)
	{
		valid = 0;
		fprintf(stderr, "AVL tree failure: height difference %d doesn't match balance-factor of %d at node %p\n",
			diffheight, this->balance, this->data);
	}

	/* Verify that search-tree property is satisfied */

	if (this->subtree[LEFT] &&
	   (compare(this->subtree[LEFT], comparefunc, this->data, EQ_CMP) == -1))
	{
		valid = 0;
		fprintf(stderr, "AVL tree failure: node %p is *smaller* than left subtree %p\n",
			this->data, this->subtree[LEFT]->data);
	}

	if (this->subtree[RIGHT] &&
	   (compare(this->subtree[RIGHT], comparefunc, this->data, EQ_CMP) == 1))
	{
		valid = 0;
		fprintf(stderr, "AVL tree failure: node %p is *greater* than right subtree %p\n",
			this->data, this->subtree[LEFT]->data);
	}

	return  valid;
}

#ifdef DEBUG

/* --- Routines for dumping the tree ------------------------------------- */

enum {
	LTREE,
	KEY,
	RTREE
};

static void indent(int len)
{
	int i;

	for (i=0; i<len; i++)
		putchar(' ');
}

static void dump(struct avlnode* node, avl_item_cb_t displayfunc, int order, int level)
{
	unsigned len = (level * 5) + 1;

	if ((order == LTREE) && (node->subtree[LEFT] == NULL))
	{
		indent(len);
		printf("**NULL**\n");
	}
	else if (order == KEY)
	{
		indent(len);
		printf("[%x] ", (int32)node);
		displayfunc(node->data);
		printf(" : %d\n", node->balance);
	}
	if ((order == RTREE) && (node->subtree[RIGHT] == NULL))
	{
		indent(len);
		printf("**NULL**\n");
	}
}

void avl_dump(struct avlnode* node, avl_item_cb_t displayfunc, int level)
{
	if (node == NULL)
		printf("***EMPTY TREE***\n");
	else
	{
		dump(node, displayfunc, RTREE, level);
		if (node->subtree[RIGHT])
			avl_dump(node->subtree[RIGHT], displayfunc, level+1);
		dump(node, displayfunc, KEY, level);
		if (node->subtree[LEFT])
			avl_dump(node->subtree[LEFT], displayfunc, level+1);
		dump(node, displayfunc, LTREE, level);
	}
}
#endif
