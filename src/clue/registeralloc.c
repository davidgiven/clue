/* registeralloc.c
 * Register allocation
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

static void wire_up_bb_list(struct basic_block_list *list,
		unsigned long generation);

struct hardreg hardregs[NUM_REGS];

enum
{
	TAG_MORIBUND = 1,
	TAG_DIRTY
};

void init_register_allocator(void)
{
	int i;

	for (i=0; i<NUM_REGS; i++)
		hardregs[i].number = i;
}

/* Generate a string name of a hardreg. */

const char* show_hardreg(struct hardreg* reg)
{
	return aprintf("H%d", reg->number);
}

/* Reset all hardregs to empty. */

void reset_hardregs(void)
{
	int i;
	for (i = 0; i < NUM_REGS; i++)
	{
		struct hardreg* reg = &hardregs[i];

		reg->contains = NULL;
		reg->busy = reg->dying = reg->used = 0;
	}
}

/* Mark all hardregs as untouched. */

void untouch_hardregs(void)
{
	int i;
	for (i = 0; i < NUM_REGS; i++)
	{
		struct hardreg* reg = &hardregs[i];

		reg->touched = 0;
	}
}

/* Allocate an unused hardreg. */

struct hardreg* allocate_hardreg(struct bb_state* state)
{
	int i;
	for (i = 0; i < NUM_REGS; i++)
	{
		struct hardreg* reg = &hardregs[i];

		if (!reg->contains)
		{
			reg->touched = 1;
			return reg;
		}
	}
	return NULL;
}

/* Marks a hardreg as containing a particular pseudo. */

void put_pseudo_in_hardreg(struct bb_state *state, pseudo_t pseudo,
		struct hardreg* reg)
{
	EC("-- pseudo %s assigned to hardreg %s\n", show_pseudo(pseudo),
			show_hardreg(reg));

	struct pinfo* pinfo = lookup_pinfo_of_pseudo(pseudo);
	assert(!pinfo->reg);

	pinfo->reg = reg;

	add_ptr_list(&reg->contains, pinfo);
}

/* Marks a pseudo as moribund, so the next call to kill_moribund_pseudos()
 * will kill it, causing the register it's stored in to be freed.
 */

void mark_pseudo_as_dead(struct bb_state* state, pseudo_t pseudo)
{
	struct hardreg* reg = find_source_hardreg_for_pseudo(state, pseudo);
	if (!reg)
		return;

	if (!reg->contains)
		return;

	struct pinfo* pinfo;
	FOR_EACH_PTR(reg->contains, pinfo)
	{
		if (pinfo->pseudo == pseudo)
		{
			EC("-- pseudo %s in hardreg %s is dying\n",
					show_pseudo(pseudo), show_hardreg(reg));
			pinfo->dying = 1;
			reg->dying = 1;
		}
	}
	END_FOR_EACH_PTR(pinfo);
}

/* Removes any dying pseudos from a specific hardreg. */

void kill_dying_hardreg(struct hardreg *reg)
{
	if (reg->dying && reg->contains)
	{
		struct pinfo* pinfo;
		FOR_EACH_PTR(reg->contains, pinfo)
		{
			if (pinfo->dying)
			{
				EC("-- pseudo %s has died and is no longer in hardreg %s\n",
						show_pseudo(pinfo->pseudo), show_hardreg(reg));

				pinfo->dying = 0;
				pinfo->reg = NULL;
			}
		}
		END_FOR_EACH_PTR(pinfo);

		free_ptr_list(&reg->contains);
		reg->dying = 0;
	}
}

/* Finally kill any pseudos that have been dying. */

void kill_dying_pseudos(struct bb_state* state)
{
	int i;
	for (i = 0; i < NUM_REGS; i++)
		kill_dying_hardreg(&hardregs[i]);
}

/* For a pseudo that's going to be read from, finds the hardreg it's stored
 * in. If the pseudo isn't in a hardreg, return NULL. */

struct hardreg* find_source_hardreg_for_pseudo(struct bb_state* state,
		pseudo_t pseudo)
{
	struct pinfo* pinfo = lookup_pinfo_of_pseudo(pseudo);
	if (pinfo->reg)
		return pinfo->reg;

	return NULL;
}

/* In order for basic blocks to talk to each other, we need to wire together
 * their inputs and outputs. We have to do this all in one go otherwise we'll
 * end up confusing ourselves. Once done, the code generator can allocate
 * through the gaps.
 *
 * This function uses pinfo to keep track of the registers, so you'll need to
 * call reset_pinfo() once done.
 * */

static void wire_up_storage_hash_list(struct storage_hash_list* list)
{
	struct storage_hash *entry;
	FOR_EACH_PTR(list, entry)
	{
		struct storage* storage = entry->storage;
		pseudo_t pseudo = entry->pseudo;
		struct pinfo* pinfo = lookup_pinfo_of_pseudo(pseudo);

		if ((!pinfo->wire) && (!pinfo->stacked))
		{
			switch (storage->type)
			{
				case REG_REG:
					/* The front end wants this to be in a specific register. */
					pinfo->wire = &hardregs[storage->regno];
					printf("-- pseudo %s ==> hardreg %s (%p)\n",
							show_pseudo(entry->pseudo),
							show_hardreg(pinfo->wire), storage);

					put_pseudo_in_hardreg(NULL, entry->pseudo, pinfo->wire);
					break;

				case REG_UDEF:
					/* The front end doesn't care where this is. */
					pinfo->wire = allocate_hardreg(NULL);
					printf("-- pseudo %s ==> hardreg %s (%p)\n",
							show_pseudo(entry->pseudo),
							show_hardreg(pinfo->wire), storage);

					put_pseudo_in_hardreg(NULL, entry->pseudo, pinfo->wire);
					break;

				default:
					/* Shouldn't be anything else... */
					printf("-- pseudo %s in storage %s?\n",
							show_pseudo(entry->pseudo),
							show_storage(storage));
					assert(0);

			}
		}
	}
	END_FOR_EACH_PTR(entry);

}

void wire_up_bb_recursively(struct basic_block* bb, unsigned long generation)
{
	bb->generation = generation;

	/* Ensure that the parent bbs of this one get generated first. */

	wire_up_bb_list(bb->parents, generation);
	wire_up_storage_hash_list(gather_storage(bb, STOR_IN));
	wire_up_storage_hash_list(gather_storage(bb, STOR_OUT));
	wire_up_bb_list(bb->children, generation);
}

static void wire_up_bb_list(struct basic_block_list* list,
		unsigned long generation)
{
	struct basic_block *bb;
	FOR_EACH_PTR(list, bb)
	{
		if (bb->generation == generation)
			continue;
		wire_up_bb_recursively(bb, generation);
	}
	END_FOR_EACH_PTR(bb);
}

