/* Hacked CLBG benchmark.
 *
 * This file is available under the Revised BSD open source license. To get
 * the full license text, see:
 * http://shootout.alioth.debian.org/gp4/miscfile.php?file=license&title=revised%20BSD%20license
 *
 * $Id$
 * $HeadURL$
 * $LastChangedDate: 2007-04-30 22:41:42 +0000 (Mon, 30 Apr 2007) $
 */

/* The Computer Language Shootout Benchmarks
   http://shootout.alioth.debian.org/

   contributed by Kevin Carson
   compilation:
       gcc -O3 -fomit-frame-pointer -funroll-loops -static binary-trees.c -lm
       icc -O3 -ip -unroll -static binary-trees.c -lm
*/

#include <malloc.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include "clbg.h"

typedef struct tn {
    struct tn*    left;
    struct tn*    right;
    long          item;
} treeNode;


static treeNode* NewTreeNode(treeNode* left, treeNode* right, long item)
{
    treeNode*    new;

    new = (treeNode*)malloc(sizeof(treeNode));

    new->left = left;
    new->right = right;
    new->item = item;

    return new;
} /* NewTreeNode() */


static long ItemCheck(treeNode* tree)
{
    if (tree->left == NULL)
        return tree->item;
    else
        return tree->item + ItemCheck(tree->left) - ItemCheck(tree->right);
} /* ItemCheck() */


static treeNode* BottomUpTree(long item, unsigned depth)
{
    if (depth > 0)
        return NewTreeNode
        (
            BottomUpTree(2 * item - 1, depth - 1),
            BottomUpTree(2 * item, depth - 1),
            item
        );
    else
        return NewTreeNode(NULL, NULL, item);
} /* BottomUpTree() */


static void DeleteTree(treeNode* tree)
{
    if (tree->left != NULL)
    {
        DeleteTree(tree->left);
        DeleteTree(tree->right);
    }

    free(tree);
} /* DeleteTree() */


int clbgmain(int argc, const char* argv[])
{
    unsigned   N, depth, minDepth, maxDepth, stretchDepth;
    treeNode   *stretchTree, *longLivedTree, *tempTree;

    N = atol(argv[1]);

    minDepth = 4;

    if ((minDepth + 2) > N)
        maxDepth = minDepth + 2;
    else
        maxDepth = N;

    stretchDepth = maxDepth + 1;

    stretchTree = BottomUpTree(0, stretchDepth);
    printf
    (
        "stretch tree of depth %u\t check: %i\n",
        stretchDepth,
        ItemCheck(stretchTree)
    );

    DeleteTree(stretchTree);

    longLivedTree = BottomUpTree(0, maxDepth);

    for (depth = minDepth; depth <= maxDepth; depth += 2)
    {
        long    i, iterations, check;

        iterations = pow(2, maxDepth - depth + minDepth);

        check = 0;

        for (i = 1; i <= iterations; i++)
        {
            tempTree = BottomUpTree(i, depth);
            check += ItemCheck(tempTree);
            DeleteTree(tempTree);

            tempTree = BottomUpTree(-i, depth);
            check += ItemCheck(tempTree);
            DeleteTree(tempTree);
        } /* for(i = 1...) */

        printf
        (
            "%i\t trees of depth %u\t check: %i\n",
            iterations * 2,
            depth,
            check
        );
    } /* for(depth = minDepth...) */

    printf
    (
        "long lived tree of depth %u\t check: %i\n",
        maxDepth,
        ItemCheck(longLivedTree)
    );

    return 0;
} /* main() */
