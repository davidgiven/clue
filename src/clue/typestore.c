/* typestore.c
 * Type auxiliary storage
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

/* sparse doesn't make it very easy to determine the basic type of a pseudo
 * after the pseudo's been defined. So, we keep track of the types ourselves
 * in the pinfo structure.
 */

int get_base_type_of_symbol(struct symbol* s)
{
	assert(s);
	if (!s)
		return TYPE_ANY;
	if (s->type == SYM_ARRAY)
		return TYPE_PTR;
	if (s->type == SYM_PTR)
		return TYPE_PTR;
	if (s->type == SYM_FN)
		return TYPE_PTR;
	if (s == &int_type)
		return TYPE_INT;
	if (s == &fp_type)
		return TYPE_FLOAT;
	if (s == &ptr_ctype)
		return TYPE_PTR;

	s = s->ctype.base_type;
	if (s)
		return get_base_type_of_symbol(s);
	assert(0);
}

static int get_base_type_of_instruction(struct instruction* insn)
{
	pseudo_t p;

	if (!insn)
	{
		printf("failed to get type of instruction!\n");
		return TYPE_ANY;
	}

	switch (insn->opcode)
	{
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
		case OP_AND_BOOL:
		case OP_OR_BOOL:
			return TYPE_INT;

		case OP_ADD:
		{
			int type1 = get_base_type_of_pseudo(insn->src1);
			int type2 = get_base_type_of_pseudo(insn->src2);

			if ((type1 == TYPE_PTR) || (type2 == TYPE_PTR))
				return TYPE_PTR;
			return type1;
		}

		case OP_SUB:
		{
			int type1 = get_base_type_of_pseudo(insn->src1);
			int type2 = get_base_type_of_pseudo(insn->src2);

			if ((type1 == TYPE_PTR) && (type2 == TYPE_PTR))
				return TYPE_INT;
			else if ((type1 == TYPE_PTR) || (type2 == TYPE_PTR))
				return TYPE_PTR;
			return type1;
		}

	}

	if (insn->type)
		return get_base_type_of_symbol(insn->type);

	switch (insn->opcode)
	{
		case OP_SEL:
			p = insn->src2;
			break;

		case OP_COPY:
			p = insn->src;
			break;

		case OP_SETVAL:
			if (insn->symbol)
				p = insn->symbol;
			else
				return get_base_type_of_symbol(insn->val->ctype);
			break;
#if 0
		/* These instructions don't really have types. */

		case OP_ENTRY:
			assert(0);

		case OP_DEATHNOTE:
			return TYPE_ANY;

		case OP_LOAD:
			assert(0);
			break;

		case OP_STORE:
		case OP_NEG:
		case OP_COPY:
		case OP_RET:
			p = insn->src;
			break;

		case OP_MULU:
		case OP_MULS:
		case OP_AND:
		case OP_OR:
		case OP_XOR:
		case OP_AND_BOOL:
		case OP_OR_BOOL:
		case OP_DIVU:
		case OP_DIVS:
		case OP_MODU:
		case OP_MODS:
		case OP_SHL:
		case OP_LSR:
		case OP_ASR:
			p = insn->src1;
			break;

		case OP_PHI:
			PREPARE_PTR_LIST(insn->phi_list, p);
			FINISH_PTR_LIST(p);
			break;

		case OP_PHISOURCE:
			p = insn->phi_src;
			break;

		case OP_CALL:
			return get_base_type_of_symbol(insn->func->sym->ctype.base_type);

		case OP_SCAST:
			return get_base_type_of_symbol(insn->orig_type);

#endif

		default:
			printf("don't know how to get the type of:\n%s\n", show_instruction(insn));
			assert(0);
			break;
	}

	return get_base_type_of_pseudo(p);
}

int get_base_type_of_pseudo(pseudo_t pseudo)
{
	struct pinfo* pinfo = lookup_pinfo_of_pseudo(pseudo);
	if (pinfo->type)
		return pinfo->type;

	switch (pseudo->type)
	{
		case PSEUDO_VOID:
			return TYPE_INT;
			//die("unable to proceed due to errors");

		case PSEUDO_REG:
		case PSEUDO_PHI:
			pinfo->type = get_base_type_of_instruction(pseudo->def);
			break;

		case PSEUDO_ARG:
		{
			/* Get the entrypoint to this function via the pseudo. */

			struct entrypoint* ep = pseudo->def->bb->ep;

			/* Now find the function symbol itself (which has the argument
			 * data attached to it). */

			struct symbol* function = get_base_type(ep->name);

			/* Iterate through the arguments until we find this one. */

			int count = 0;
			struct symbol* type;
			PREPARE_PTR_LIST(function->arguments, type);
			for (count = 1; count < pseudo->nr; count++)
				NEXT_PTR_LIST(type);
			FINISH_PTR_LIST(type);

			/* Now extract the base type from the argument. */

			pinfo->type = get_base_type_of_symbol(type);
			break;
		}

		case PSEUDO_SYM:
			pinfo->type = TYPE_PTR;
			break;

		case PSEUDO_VAL:
			pinfo->type = TYPE_INT;
			break;

		default:
			printf("unknown pseudo type %d!\n", pseudo->type);
			assert(0);
	}

	return pinfo->type;
}
