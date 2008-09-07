/* avl.h
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


/* Represents a single node of the tree */

struct avlnode
{
#ifdef DEBUG
	int magic;
#endif
	void *data;
	struct avlnode* subtree[2];
	int balance;
};

typedef struct avlnode* avltree_t;

/* Various callbacks */

typedef int (*avl_comparison_cb_t)(const void*, const void*);
typedef void (*avl_item_cb_t)(void*);

/* Prototypes */

extern void* avl_search(avltree_t root, avl_comparison_cb_t comparefunc,
	void* target, int cmp);
extern void* avl_insert(avltree_t* rootp, avl_comparison_cb_t comparefunc,
	void* item, int* change);
extern void* avl_delete(avltree_t* rootp, avl_comparison_cb_t comparefunc,
	void* item, int* change, int cmp);
extern void avl_nuke(avltree_t root, avl_item_cb_t deletefunc);
extern int avl_validate(avltree_t this, avl_comparison_cb_t comparefunc);
extern void avl_traverse(avltree_t node, avl_item_cb_t displayfunc);

#ifdef DEBUG

/* Node recognition magic number */

#define AVLTREE_MAGIC (0x11AA2233)

/* Prototypes */

extern void avl_dump(struct avlnode* node, avl_item_cb_t displayfunc, int level);

#endif
