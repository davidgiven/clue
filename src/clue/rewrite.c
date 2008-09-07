/* rewrite.c
 * Instruction list rewriter
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

/* sparse likes multi-addressing-mode instructions, where one instruction is
 * used with several different types of pseudo (expression results, constants,
 * constants of the wrong type, etc). This code decomposes these instructions
 * into more primitive forms so that the constants get loaded into their
 * own pseudos before use. */

static void rewrite_bb_list(struct basic_block_list *list,
		unsigned long generation);

struct decompose
{
	struct instruction* insn;
	pseudo_t pseudo;
};

static struct decompose decompose_pseudo(struct instruction* insn,
		pseudo_t pseudo, int desiredtype, struct symbol* ctype)
{
	int ptype = get_base_type_of_pseudo(pseudo);
	struct decompose d;
	d.insn = NULL;
	d.pseudo = NULL;

	if ((desiredtype != TYPE_ANY) &&
		(ptype != desiredtype))
	{
		switch (desiredtype)
		{
			case TYPE_PTR:
			case TYPE_FNPTR:
			{
				/* Only constant 0 literals can be turned into pointers. */

				if ((pseudo->type != PSEUDO_VAL) ||
					(pseudo->value != 0))
					sparse_error(insn->pos,
							"only literal 0 values may be cast to pointers");

				struct expression* e = alloc_const_expression(insn->pos, 0);
				e->ctype = &null_ctype;

				d.insn = __alloc_instruction(0);
				d.pseudo = alloc_pseudo(d.insn);
				d.insn->opcode = OP_SETVAL;
				d.insn->size = insn->size;
				d.insn->bb = insn->bb;
				d.insn->pos = insn->pos;
				d.insn->target = d.pseudo;
				d.insn->val = e;
				return d;
			}
		}
	}

	if (!is_pseudo_in_register(pseudo))
	{
		d.insn = __alloc_instruction(0);
		d.pseudo = alloc_pseudo(d.insn);
		d.insn->opcode = OP_COPY;
		d.insn->size = insn->size;
		d.insn->bb = insn->bb;
		d.insn->pos = insn->pos;
		d.insn->target = d.pseudo;
		d.insn->src = pseudo;
		d.insn->type = ctype;
	}

//	if (d.pseudo)
//add_pseudo(&insn->bb->needs, d.pseudo);
	return d;
}

#define DECOMPOSE(SRC, RTYPE, CTYPE) \
	do { \
		d = decompose_pseudo(insn, SRC, RTYPE, CTYPE); \
		if (d.pseudo) \
			SRC = d.pseudo; \
		if (d.insn) \
			INSERT_CURRENT(d.insn, insn); \
	} while(0)

static void rewrite_bb(struct basic_block* bb)
{
	struct instruction *insn;
	FOR_EACH_PTR(bb->insns, insn)
	{
		if (!insn->bb)
			continue;

		struct decompose d;
	again:
		switch (insn->opcode)
		{
			case OP_COPY:
				break;

			case OP_RET:
			case OP_CAST:
			case OP_SCAST:
			case OP_FPCAST:
			case OP_PTRCAST:
			{
				if (insn->src)
					DECOMPOSE(insn->src, TYPE_ANY, NULL);
				break;
			}

			case OP_ADD:
			case OP_MULU:
			case OP_MULS:
			case OP_AND:
			case OP_OR:
			case OP_XOR:
			case OP_AND_BOOL:
			case OP_OR_BOOL:
			case OP_SUB:
			case OP_DIVU:
			case OP_DIVS:
			case OP_MODU:
			case OP_MODS:
			case OP_SHL:
			case OP_LSR:
			case OP_ASR:
			case OP_SET_GT:
			case OP_SET_GE:
			case OP_SET_LT:
			case OP_SET_LE:
			case OP_SET_EQ:
			case OP_SET_NE:
			case OP_SET_B:
			case OP_SET_A:
			case OP_SET_BE:
			case OP_SET_AE:
			{
				DECOMPOSE(insn->src1, TYPE_ANY, NULL);
				DECOMPOSE(insn->src2, TYPE_ANY, NULL);
				break;
			}

			case OP_SEL:
			{
				DECOMPOSE(insn->src1, TYPE_ANY, NULL);
				DECOMPOSE(insn->src2, TYPE_ANY, NULL);
				DECOMPOSE(insn->src3, TYPE_ANY, NULL);
				break;
			}

			case OP_CALL:
			{
				struct symbol* sym;
				if (insn->func->type == PSEUDO_REG)
					sym = insn->func->def->type;
				else
				{
					sym = insn->func->sym;
					while (sym->type != SYM_FN)
					{
						if (sym->type == SYM_NODE)
							sym = sym->ctype.base_type;
						else if (sym->type == SYM_PTR)
							sym = get_base_type(sym);
						else
							assert(0);
					}
				}

				DECOMPOSE(insn->func, TYPE_ANY, sym);

				struct symbol* type;
				pseudo_t arg;
				PREPARE_PTR_LIST(sym->arguments, type);
				FOR_EACH_PTR(insn->arguments, arg)
				{
					int decltype = TYPE_ANY;
					if (type)
						decltype = get_base_type_of_symbol(type);

					pseudo_t newarg = arg;
					DECOMPOSE(newarg, decltype, NULL);
					REPLACE_CURRENT_PTR(arg, newarg);

					NEXT_PTR_LIST(type);
				}
				END_FOR_EACH_PTR(arg);
				FINISH_PTR_LIST(type);

				break;
			}

			case OP_BR:
			{
				if (insn->cond)
					DECOMPOSE(insn->cond, TYPE_ANY, NULL);
				break;
			}

			case OP_LOAD:
			case OP_STORE:
			{
				/* Structure copies have a few special requirements that need
				 * handling: they can't handle offsets, and as the target
				 * pseudo represents a non-scalar we musn't touch it. */

				int dtype = get_base_type_of_pseudo(insn->target);
				if ((dtype == TYPE_STRUCT) && (insn->offset > 0))
				{
					d.insn = __alloc_instruction(0);
					pseudo_t value = alloc_pseudo(d.insn);
					value->type = PSEUDO_VAL;
					value->value = insn->offset;

					d.pseudo = alloc_pseudo(d.insn);
					d.insn->opcode = OP_ADD;
					d.insn->size = bits_in_pointer;
					d.insn->bb = insn->bb;
					d.insn->pos = insn->pos;
					d.insn->target = d.pseudo;
					d.insn->src1 = insn->src;
					d.insn->src2 = value;
					d.insn->type = &ptr_ctype;

					insn->src = d.pseudo;
					insn->offset = 0;
					INSERT_CURRENT(d.insn, insn);
					insn = d.insn;
					goto again;
				}

				DECOMPOSE(insn->src, TYPE_PTR, NULL);
				if (dtype != TYPE_STRUCT)
					DECOMPOSE(insn->target, TYPE_ANY, NULL);

				break;
			}

#if 0
			case OP_PHISOURCE:
			{
				DECOMPOSE(insn->phi_src, TYPE_ANY, NULL);

				struct instruction* phi;
				int i = 0;
				FOR_EACH_PTR(insn->phi_users, phi) {
					pseudo_t tmp = phi->src;
					pseudo_t src = insn->phi_src;

					if (i == 0)
					{
						insn->opcode = OP_COPY;
						insn->target = tmp;
						insn->src = src;
					}
					else
					{
						struct instruction *copy = __alloc_instruction(0);

						copy->bb = bb;
						copy->opcode = OP_COPY;
						copy->size = insn->size;
						copy->pos = insn->pos;
						copy->target = tmp;
						copy->src = src;

						INSERT_CURRENT(copy, insn);
					}
#if 0
					// update the liveness info
					remove_phisrc_defines(insn);
					// FIXME: should really something like add_pseudo_exclusive()
					add_pseudo(&bb->defines, tmp);
#endif

					i++;
				}
				END_FOR_EACH_PTR(phi);
				break;
			}
#endif
		}
	}
	END_FOR_EACH_PTR(insn);
}

void rewrite_bb_recursively(struct basic_block* bb, unsigned long generation)
{
	bb->generation = generation;

	/* Ensure that the parent bbs of this one get generated first. */

	rewrite_bb_list(bb->parents, generation);
	rewrite_bb(bb);
	rewrite_bb_list(bb->children, generation);
}

static void rewrite_bb_list(struct basic_block_list* list,
		unsigned long generation)
{
	struct basic_block *bb;
	FOR_EACH_PTR(list, bb)
	{
		if (bb->generation == generation)
			continue;
		rewrite_bb_recursively(bb, generation);
	}
	END_FOR_EACH_PTR(bb);
}

