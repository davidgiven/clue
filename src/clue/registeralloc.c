/* registeralloc.c
 * Register allocation
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

static void wire_up_bb_list(struct basic_block_list *list,
		unsigned long generation, int* sequence);

struct hardreg stackbase_reg;
struct hardreg stackoffset_reg;
struct hardreg frameoffset_reg;
struct hardreg hardregs[NUM_REGS];
static struct pinfo_list* dying_pinfos = NULL;

const static int type_to_regtype[] =
{
	[TYPE_INT] = REGTYPE_INT,
	[TYPE_FLOAT] = REGTYPE_FLOAT,
	[TYPE_PTR] = REGTYPE_INT, /* actually refers to the offset part */
	[TYPE_FNPTR] = REGTYPE_FPTR,
	[TYPE_FN] = REGTYPE_FPTR,
};

void init_register_allocator(void)
{
	int i;

	for (i=0; i<NUM_REGS; i++)
		hardregs[i].number = i;

	stackbase_reg.regclass = find_regclass_for_regtype(REGTYPE_OPTR);
	stackoffset_reg.regclass = find_regclass_for_regtype(REGTYPE_INT);
	frameoffset_reg.regclass = find_regclass_for_regtype(REGTYPE_INT);
}

/* Generate a string name of a hardreg. */

const char* show_hardreg(struct hardreg* reg)
{
	if (reg == &stackbase_reg)
		return cg->stackname;
	else if (reg == &stackoffset_reg)
		return cg->spname;
	else if (reg == &frameoffset_reg)
		return cg->fpname;
	else
		return cg->get_register_name(reg);
}

/* Generate a string name of a hardregref. */

const char* show_hardregref(struct hardregref* hrf)
{
	static const char* typenames[] = {
		[TYPE_NONE] = "none",
		[TYPE_ANY] = "any",
		[TYPE_INT] = "int",
		[TYPE_FLOAT] = "float",
		[TYPE_PTR] = "ptr",
		[TYPE_FNPTR] = "fnptr",
		[TYPE_STRUCT] = "struct",
	};

	const char* s = typenames[hrf->type];
	assert(s);

	switch (hrf->type)
	{
		case TYPE_NONE:
			return "[none]";

		case TYPE_PTR:
			return aprintf("[%s:%s/%d+%s/%d]", s,
					show_hardreg(hrf->base), hrf->base->busy,
					show_hardreg(hrf->simple), hrf->simple->busy);

		default:
			return aprintf("[%s:%s/%d]", s,
					show_hardreg(hrf->simple), hrf->simple->busy);
	}
}

/* Reset all hardregs to empty. */

void reset_hardregs(void)
{
	int i;
	for (i = 0; i < NUM_REGS; i++)
	{
		struct hardreg* reg = &hardregs[i];

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

		reg->name = NULL;
		reg->regclass = NUM_REG_CLASSES;
		reg->touched = 0;
	}

	cg->reset_registers();
}

/* Convert a return type to a register class. */

int find_regclass_for_returntype(int type)
{
	switch (type)
	{
		case TYPE_VOID:      return REGCLASS_VOID;
		case TYPE_PTR:       return REGCLASS_REGPAIR;
		case TYPE_INT:       return find_regclass_for_regtype(REGTYPE_INT);
		case TYPE_FLOAT:     return find_regclass_for_regtype(REGTYPE_FLOAT);
		case TYPE_FNPTR:     return find_regclass_for_regtype(REGTYPE_FPTR);

		default:
			assert(0);
	}
}

/* Determines what register class a given register should have. */

int find_regclass_for_regtype(int regtype)
{
	int i = 0;
	for (i = 0; i < NUM_REG_CLASSES; i++)
		if (cg->register_class[i] & regtype)
			return i;
	assert(0);
	return 0;
}

/* Allocate an unused hardreg. */

struct hardreg* allocate_hardreg(int regtype)
{
	/* Look for a register that's unused and can store the given type
	 * (either because it's an allocated register with a matching
	 * register class or because it's an unallocated register).
	 */

	int i;
	for (i = 0; i < NUM_REGS; i++)
	{
		struct hardreg* reg = &hardregs[i];

		if ((reg->busy == 0) &&
			((reg->regclass == NUM_REG_CLASSES) ||
					(cg->register_class[reg->regclass] & regtype)))
		{
			reg->busy = 1;
			reg->touched = 1;
			if (!reg->name)
			{
				reg->regclass = find_regclass_for_regtype(regtype);

				cg->init_register(reg, reg->regclass);

				cg->comment("hardreg %s assigned to register class %d\n",
						show_hardreg(reg), reg->regclass);
			}
			return reg;
		}
	}
	return NULL;
}

/* Find the hardregref containing a particular pseudo. */

void find_hardregref(struct hardregref* hrf, pseudo_t pseudo)
{
	assert(is_pseudo_in_register(pseudo));

	struct pinfo* pinfo = lookup_pinfo_of_pseudo(pseudo);
	assert(pinfo->reg.type);

	*hrf = pinfo->reg;
}

/* Creates a hardregref and binds it to a particular pseudo. */

void create_hardregref(struct hardregref* hrf, pseudo_t pseudo)
{
	struct pinfo* pinfo = lookup_pinfo_of_pseudo(pseudo);

	if (pinfo->reg.type == TYPE_NONE)
	{
		/* This pseudo has not been placed in a register. */

		int regtype = type_to_regtype[pinfo->type];
		assert(regtype);

		hrf->type = pinfo->type;
		hrf->simple = allocate_hardreg(regtype);
		if (hrf->type == TYPE_PTR)
			hrf->base = allocate_hardreg(REGTYPE_OPTR);
		else
			hrf->base = NULL;
		put_pseudo_in_hardregref(pseudo, hrf);
	}
	else
	{
		*hrf = pinfo->reg;
		ref_hardregref(hrf);
	}

}

/* Creates a new hardregref that shares the .base register with another
 * one. */

void clone_ptr_hardregref(struct hardregref* src, struct hardregref* hrf,
		pseudo_t pseudo)
{
	assert(pseudo->type == PSEUDO_REG);
	assert(src->type == TYPE_PTR);

	struct pinfo* pinfo = lookup_pinfo_of_pseudo(pseudo);
	if (pinfo->reg.type == TYPE_NONE)
	{
		hrf->type = pinfo->type;
		hrf->simple = allocate_hardreg(REGTYPE_INT);
		hrf->base = src->base;
		hrf->base->busy++;
		put_pseudo_in_hardregref(pseudo, hrf);
	}
	else
	{
		*hrf = pinfo->reg;
		ref_hardregref(hrf);
	}
}

/* Reference and unreference a hardregref. */

void ref_hardregref(struct hardregref* hrf)
{
	switch (hrf->type)
	{
		case TYPE_NONE:
			break;

		case TYPE_PTR:
			hrf->base->busy++;
			/* fall through */

		default:
			hrf->simple->busy++;
	}
}

void unref_hardregref(struct hardregref* hrf)
{
	switch (hrf->type)
	{
		case TYPE_NONE:
			break;

		case TYPE_PTR:
			assert(hrf->base->busy > 0);
			hrf->base->busy--;
			if (hrf->base->busy == 0)
				cg->comment("hardreg %s now unused\n", show_hardreg(hrf->base));
			/* fall through */

		default:
			assert(hrf->simple->busy > 0);
			hrf->simple->busy--;
			if (hrf->simple->busy == 0)
				cg->comment("hardreg %s now unused\n", show_hardreg(hrf->simple));
	}
}

void unref_hardreg(struct hardreg* reg)
{
	assert(reg->busy > 0);
	reg->busy--;
}

/* Marks a pseudo as being stored in a particular hardregref. */

void put_pseudo_in_hardregref(pseudo_t pseudo, struct hardregref* hrf)
{
	cg->comment("pseudo %s assigned to hardregref %s\n", show_pseudo(pseudo),
			show_hardregref(hrf));

	struct pinfo* pinfo = lookup_pinfo_of_pseudo(pseudo);
	assert(!pinfo->reg.type);

	pinfo->reg = *hrf;
}

/* Marks a pseudo as moribund, so the next call to kill_moribund_pseudos()
 * will kill it, causing the register it's stored in to be freed.
 */

void mark_pseudo_as_dying(pseudo_t pseudo)
{
	struct pinfo* pinfo = lookup_pinfo_of_pseudo(pseudo);
	pinfo->dying = 1;

	cg->comment("pseudo %s in hardregref %s is dying\n",
			show_pseudo(pseudo), show_hardregref(&pinfo->reg));

	add_ptr_list(&dying_pinfos, pinfo);
}

/* Finally kills any dying pseudos. */

void kill_dying_pseudos(void)
{
	struct pinfo* pinfo;
	FOR_EACH_PTR(dying_pinfos, pinfo)
	{
		assert(pinfo->dying);

		cg->comment("pseudo %s has died and is no longer in hardregref %s\n",
				show_pseudo(pinfo->pseudo), show_hardregref(&pinfo->reg));
		unref_hardregref(&pinfo->reg);
	}
	END_FOR_EACH_PTR(pinfo);

	free_ptr_list(&dying_pinfos);
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

		if ((!pinfo->wire.type) && (!pinfo->stacked))
		{
			switch (storage->type)
			{
#if 0
				case REG_REG:
					assert(0);
					/* The front end wants this to be in a specific register. */
					pinfo->wire = &hardregs[storage->regno];
					printf("pseudo %s ==> hardreg %s (%p)\n",
							show_pseudo(entry->pseudo),
							show_hardreg(pinfo->wire), storage);

					put_pseudo_in_hardreg(NULL, entry->pseudo, pinfo->wire);
					break;
#endif
				case REG_ARG:
					/* This is an argument; if there's a specified register,
					 * use it.
					 */

					if (!pinfo->stacked)
						break;
					/* fall through */

				case REG_UDEF:
					/* The front end doesn't care where this is. */

					create_hardregref(&pinfo->wire, pseudo);
					cg->comment("pseudo %s ==> hardregref %s\n",
							show_pseudo(entry->pseudo),
							show_hardregref(&pinfo->wire));
#if 0
					pinfo->wire = allocate_hardreg(NULL);
					cg->comment("pseudo %s ==> hardreg %s (%p)\n",
							show_pseudo(entry->pseudo),
							show_hardreg(pinfo->wire), storage);

					put_pseudo_in_hardreg(NULL, entry->pseudo, pinfo->wire);
#endif
					break;

				default:
					/* Shouldn't be anything else... */
					cg->comment("pseudo %s in storage %s?\n",
							show_pseudo(entry->pseudo),
							show_storage(storage));
					assert(0);

			}
		}
	}
	END_FOR_EACH_PTR(entry);

}

void wire_up_bb_recursively(struct basic_block* bb, unsigned long generation,
		int* sequence)
{
	bb->generation = generation;

	struct binfo* binfo = lookup_binfo_of_basic_block(bb);
	binfo->id = *sequence;
	(*sequence)++;

	/* Ensure that the parent bbs of this one get generated first. */

	wire_up_bb_list(bb->parents, generation, sequence);
	wire_up_storage_hash_list(gather_storage(bb, STOR_IN));
	wire_up_storage_hash_list(gather_storage(bb, STOR_OUT));
	wire_up_bb_list(bb->children, generation, sequence);
}

static void wire_up_bb_list(struct basic_block_list* list,
		unsigned long generation, int* sequence)
{
	struct basic_block *bb;
	FOR_EACH_PTR(list, bb)
	{
		if (bb->generation == generation)
			continue;
		wire_up_bb_recursively(bb, generation, sequence);
	}
	END_FOR_EACH_PTR(bb);
}

/* Is this pseudo in a register? */

int is_pseudo_in_register(pseudo_t pseudo)
{
	switch (pseudo->type)
	{
		case PSEUDO_REG:
			return 1;

		default:
			return 0;

		case PSEUDO_ARG:
		{
			struct pinfo* pinfo = lookup_pinfo_of_pseudo(pseudo);
			if (pinfo->wire.type != TYPE_NONE)
				return 1;
			return 0;
		}
	}
}

