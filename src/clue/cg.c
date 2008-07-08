/* cg.c
 * Main code generator
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

enum optype
{
	OP_REG,
	OP_VAL,
	OP_MEM,
	OP_ADDR,
	OP_STACK,
	OP_STACKI
};

struct operand
{
	enum optype type;
	union {
		struct hardreg *reg;
		long long value;
		int stackoffset; /* OP_STACK */
		struct symbol* sym; /* OP_ADDR */
	};
};

static void generate_bb_list(struct basic_block_list *list,
		unsigned long generation);

static int stacksize;

/* Emit a line to the zbuffer. */

void E(const char* format, ...)
{
	va_list ap;
	va_start(ap, format);
	zvprintf(format, ap);
	va_end(ap);
}

/* Emit a line of debug info to the zbuffer. */

void EC(const char* format, ...)
{
	if (verbose)
	{
		va_list ap;
		va_start(ap, format);
		zvprintf(format, ap);
		va_end(ap);
	}
}

/* For a pseudo that's going to be read from, finds the hardreg it's stored
 * in. If the pseudo isn't in a hardreg, allocate one, and then load the
 * pseudo into it. */

static struct hardreg* load_pseudo_into_hardreg(struct bb_state* state,
		pseudo_t pseudo)
{
	struct hardreg* reg = find_source_hardreg_for_pseudo(state, pseudo);
	if (reg)
		return reg;

	reg = allocate_hardreg(state);

	EC("-- loading pseudo %s into hardreg %s\n",
			show_pseudo(pseudo), show_hardreg(reg));
	E("%s = %s\n", show_hardreg(reg), show_simple_pseudo(state, pseudo));

	put_pseudo_in_hardreg(state, pseudo, reg);
	return reg;
}

/* For a psuedo that's going to be written to, finds the hardreg it's stored
 * in. If the pseudo isn't in a hardreg, allocate one. */

static struct hardreg* assign_pseudo_to_hardreg(struct bb_state* state,
		pseudo_t pseudo)
{
	struct hardreg* reg = find_source_hardreg_for_pseudo(state, pseudo);
	if (reg)
		return reg;

	reg = allocate_hardreg(state);

	EC("-- assigning pseudo %s to hardreg %s\n",
			show_pseudo(pseudo), show_hardreg(reg));

	put_pseudo_in_hardreg(state, pseudo, reg);
	return reg;
}

/* Creates an operand structure describing a particular input pseudo. */

static struct operand* get_source_operand(struct bb_state* state, pseudo_t pseudo)
{
	struct operand* op = malloc(sizeof(struct operand));
	memset(op, 0, sizeof(struct operand));

	struct pinfo* pinfo = lookup_pinfo_of_pseudo(pseudo);
	switch (pseudo->type)
	{
		case PSEUDO_VOID:
			return NULL;

		case PSEUDO_VAL:
			op->type = OP_VAL;
			op->value = pseudo->value;
			break;

		case PSEUDO_ARG:
		{
			assert(pinfo->stacked);

			op->type = OP_STACK;
			op->stackoffset = pinfo->stackoffset;
			break;
		}

		case PSEUDO_REG:
		{
			/* This pseudo is in (or would like to be in) a registr. */

			struct hardreg* reg = load_pseudo_into_hardreg(state, pseudo);
			if (reg)
			{
				/* Yes, it's already there. */
				op->type = OP_REG;
				op->reg = reg;
				reg->busy++;
			}
			else
			{
				/* It's not in a register yet, so load it. */
				assert(0);
			}
			break;
		}

		case PSEUDO_SYM:
		{
			if (pinfo->stacked)
			{
			stacked:
				op->type = OP_STACKI;
				op->stackoffset = pinfo->stackoffset;
				break;
			}

			struct symbol *sym = pseudo->sym;
			if (!(sym->ctype.modifiers &
					(MOD_EXTERN | MOD_TOPLEVEL | MOD_STATIC)))
			{
				/* This symbol lives on the stack. We assign them lazily. */

				pinfo->stacked = 1;
				pinfo->stackoffset = stacksize++;
				goto stacked;
			}

			/* Otherwise, this must be a static symbol. */

			op->type = OP_ADDR;
			op->sym = pseudo->sym;

			break;
		}

		default:
			assert(0);
	}

	return op;
}

/* Creates an operand structure representing a particular output pseudo. */

static struct operand* get_dest_operand(struct bb_state* state, pseudo_t pseudo)
{
	struct operand* op = malloc(sizeof(struct operand));
	memset(op, 0, sizeof(struct operand));

	switch (pseudo->type)
	{
		case PSEUDO_ARG:
			assert(0);

		case PSEUDO_REG:
		{
			/* This pseudo is in (or would like to be in) a register. */

			struct hardreg* reg = assign_pseudo_to_hardreg(state, pseudo);
			if (reg)
			{
				/* Yes, it's already there. */
				op->type = OP_REG;
				op->reg = reg;
				reg->busy++;
			}
			else
			{
				/* No registers available? */
				assert(0);
			}
			break;
		}

		default:
			assert(0);
			break;

	}

	return op;
}

/* Release an operand once we're finished with it. */

static void release_operand(struct bb_state *state, struct operand *op)
{
	switch (op->type)
	{
		case OP_REG:
			op->reg->busy--;
			break;

		case OP_ADDR:
		case OP_MEM:
			break;

		default:
			break;
	}
}

/* Render a simple source operand. */

static const char* render_simple_operand(struct operand* op)
{
	switch (op->type)
	{
		default:
			return aprintf("(unknown operand type %d)", op->type);

		case OP_REG:
			return show_hardreg(op->reg);

		case OP_VAL:
			return aprintf("%lld", op->value);

		case OP_MEM:
		case OP_ADDR:
			return show_symbol_mangled(op->sym);

		case OP_STACK:
			return aprintf("stackread(stack, fp, %d)", op->stackoffset);

		case OP_STACKI:
			return aprintf("stackoffset(stack, fp, %d)", op->stackoffset);
	}
}

/* Load data into memory. */

static void generate_load(struct instruction *insn, struct bb_state *state)
{
	struct operand* src = get_dest_operand(state, insn->target);
	struct operand* dest = get_source_operand(state, insn->src);
	//struct operand* dest = get_address_operand(state, insn);
	E("%s = ptrread(%s, %d)\n",
			render_simple_operand(src),
			render_simple_operand(dest),
			insn->offset);
	//output_insn(state, "mov.%d %s,%s", insn->size, reg_or_imm(state, insn->target), address(state, insn));
	release_operand(state, src);
	release_operand(state, dest);
}

/* Store data into memory. */

static void generate_store(struct instruction *insn, struct bb_state *state)
{
	struct operand* src = get_source_operand(state, insn->target);
	struct operand* dest = get_source_operand(state, insn->src);
	//struct operand* dest = get_address_operand(state, insn);
	E("ptrwrite(%s, %d, %s)\n",
			render_simple_operand(dest),
			insn->offset,
			render_simple_operand(src));
	//output_insn(state, "mov.%d %s,%s", insn->size, reg_or_imm(state, insn->target), address(state, insn));
	release_operand(state, src);
	release_operand(state, dest);
}

/* Load a value into a register. */

static void generate_setval(struct instruction *insn, struct bb_state *state)
{
	struct operand* dest = get_dest_operand(state, insn->target);
	pseudo_t pseudo = insn->symbol;

	if (pseudo)
	{
		struct operand* src = get_source_operand(state, pseudo);
		E("%s = %s\n", render_simple_operand(dest), render_simple_operand(src));
		release_operand(state, src);
	}
	else
	{
		int t = get_base_type_of_symbol(insn->val->ctype);
		const char* dests = render_simple_operand(dest);
		const char* srcs = show_value(insn->val);

		switch (t)
		{
			case TYPE_PTR:
				assert(insn->val->value == 0);
				E("%s = nil\n", dests);
				break;

			default:
				E("%s = %s\n", dests, srcs);
				break;
		}
	}

	release_operand(state, dest);
}

/* Cast one type to another. */

static void generate_cast(struct instruction *insn, struct bb_state *state)
{
	struct operand* src = get_source_operand(state, insn->src);
	struct operand* dest = get_dest_operand(state, insn->target);

	const char* srcs = render_simple_operand(src);
	const char* dests = render_simple_operand(dest);

	int srctype = get_base_type_of_pseudo(insn->src);
	int desttype = get_base_type_of_pseudo(insn->target);

	EC("-- cast from %d to %d\n", srctype, desttype);

	if ((srctype == TYPE_FLOAT) && (desttype == TYPE_INT))
		E("%s = int(%s)\n", dests, srcs);
	else if ((srctype == TYPE_INT) && (desttype == TYPE_FLOAT))
		E("%s = %s\n", dests, srcs);
	else if (srctype == desttype)
		E("%s = %s\n", dests, srcs);
	else
	{
		EC("!!!");
//		sparse_error(insn->src->def->pos,
//			     "this cast is not supported by clue");
	}

	release_operand(state, src);
	release_operand(state, dest);
}

/* Produce a simple 3op instruction. */

static void generate_triop(struct instruction *insn, struct bb_state *state)
{
	struct operand* src1 = get_source_operand(state, insn->src1);
	struct operand* src2 = get_source_operand(state, insn->src2);
	struct operand* src3 = get_source_operand(state, insn->src3);
	struct operand* dest = get_dest_operand(state, insn->target);

	const char* src1s = render_simple_operand(src1);
	const char* src2s = render_simple_operand(src2);
	const char* src3s = render_simple_operand(src3);
	const char* dests = render_simple_operand(dest);

	switch (insn->opcode)
	{
		case OP_SEL:
		{
			int ctype = get_base_type_of_pseudo(insn->src1);
			E("if (%s ~= %s) then %s = %s else %s = %s end\n",
					src1s,
					(ctype == TYPE_PTR) ? "nil" :
						"0",
					dests, src2s, dests, src3s);
			break;
		}
	}

	release_operand(state, src1);
	release_operand(state, src2);
	release_operand(state, dest);
}

/* Produce a simple 2op instruction. */

static void generate_binop(struct instruction *insn, struct bb_state *state)
{
	struct operand* src1 = get_source_operand(state, insn->src1);
	struct operand* src2 = get_source_operand(state, insn->src2);
	struct operand* dest = get_dest_operand(state, insn->target);

	int type1 = get_base_type_of_pseudo(insn->src1);
	int type2 = get_base_type_of_pseudo(insn->src2);

	const char* src1s = render_simple_operand(src1);
	const char* src2s = render_simple_operand(src2);
	const char* dests = render_simple_operand(dest);

	EC("-- type1=%d, type2=%d, result=%d\n", type1, type2, get_base_type_of_pseudo(insn->target));

	switch (insn->opcode)
	{
		case OP_ADD:
			if (type1 == TYPE_PTR)
				E("%s = ptroffset(%s, %s)\n", dests, src1s, src2s);
			else if (type2 == TYPE_PTR)
				E("%s = ptroffset(%s, %s)\n", dests, src2s, src1s);
			else
				E("%s = %s + %s\n", dests, src1s, src2s);
			break;

		case OP_MULU:
		case OP_MULS:
			E("%s = %s * %s\n", dests, src1s, src2s);
			break;

		case OP_AND:
			E("%s = logand(%s, %s)\n", dests, src1s, src2s);
			break;

		case OP_OR:
			E("%s = logor(%s, %s)\n", dests, src1s, src2s);
			break;

		case OP_XOR:
			E("%s = logxor(%s, %s)\n", dests, src1s, src2s);
			break;

		case OP_AND_BOOL:
			E("%s = booland(%s, %s)\n", dests, src1s, src2s);
			break;

		case OP_OR_BOOL:
			E("%s = boolor(%s, %s)\n", dests, src1s, src2s);
			break;

		case OP_SUB:
			if (type1 == TYPE_PTR)
			{
				if (type2 == TYPE_PTR)
				{
					/* Pointer difference. */

					E("%s = ptrdiff(%s, %s)\n", dests, src1s, src2s);
				}
				else
				{
					assert(type2 == TYPE_INT);
					E("%s = ptrsub(%s, %s)\n", dests, src1s, src2s);
				}
			}
			else
				E("%s = %s - %s\n", dests, src1s, src2s);
			break;

		case OP_DIVU:
		case OP_DIVS:
			if (type1 == TYPE_FLOAT)
				E("%s = %s / %s\n", dests, src1s, src2s);
			else
				E("%s = int(%s / %s)\n", dests, src1s, src2s);
			break;

		case OP_MODU:
		case OP_MODS:
			E("%s = %s %% %s\n", dests, src1s, src2s);
			break;

		case OP_SHL:
			E("%s = shl(%s, %s)\n", dests, src1s, src2s);
			break;

		case OP_LSR:
		case OP_ASR:
			E("%s = shr(%s, %s)\n", dests, src1s, src2s);
			break;

		case OP_SET_GT:
		case OP_SET_A:
			E("%s = (%s > %s) and 1 or 0\n", dests, src1s, src2s);
			break;

		case OP_SET_LT:
		case OP_SET_B:
			E("%s = (%s < %s) and 1 or 0\n", dests, src1s, src2s);
			break;

		case OP_SET_GE:
		case OP_SET_AE:
			E("%s = (%s >= %s) and 1 or 0\n", dests, src1s, src2s);
			break;

		case OP_SET_LE:
		case OP_SET_BE:
			E("%s = (%s <= %s) and 1 or 0\n", dests, src1s, src2s);
			break;

		case OP_SET_EQ:
			E("%s = (%s == %s) and 1 or 0\n", dests, src1s, src2s);
			break;

		case OP_SET_NE:
			E("%s = (%s ~= %s) and 1 or 0\n", dests, src1s, src2s);
			break;
	}

	release_operand(state, src1);
	release_operand(state, src2);
	release_operand(state, dest);
}

/* Produce a simple 1op instruction. */

static void generate_uniop(struct instruction *insn, struct bb_state *state)
{
	struct operand* src = get_source_operand(state, insn->src);
	struct operand* dest = get_dest_operand(state, insn->target);

	if (!src)
	{
		EC("-- %s is void, omitting store to %s\n",
				show_pseudo(insn->src), show_pseudo(insn->target));
		return;
	}

	const char* srcs = render_simple_operand(src);
	const char* dests = render_simple_operand(dest);

	switch (insn->opcode)
	{
		case OP_NEG:
			E("%s = -%s\n", dests, srcs);
			break;

		case OP_PHISOURCE:
		case OP_COPY:
			E("%s = %s\n", dests, srcs);
			break;
	}

	release_operand(state, src);
	release_operand(state, dest);
}

/* Return from the function. */

static void generate_ret(struct instruction *insn, struct bb_state *state)
{
	E("do return");
	if (insn->src && (insn->src->type != PSEUDO_VOID))
	{
		struct operand* src = get_source_operand(state, insn->src);
		E(" %s", render_simple_operand(src));
		release_operand(state, src);
	}
	E(" end\n");
}

/* Call a function. */

static void generate_call(struct instruction *insn, struct bb_state *state)
{
	struct operand* target = NULL;
	struct operand* function = get_source_operand(state, insn->func);
	struct operand* args[MAX_ARGS];

	memset(args, 0, sizeof(args));

	/* Collect operands for each argument. */

	pseudo_t arg;
	int arguments = 0;
	FOR_EACH_PTR(insn->arguments, arg)
	{
		args[arguments] = get_source_operand(state, arg);
		arguments++;
	}
	END_FOR_EACH_PTR(arg);

	/* Emit the instruction. */

	if (insn->target && (insn->target != VOID))
	{
		target = get_dest_operand(state, insn->target);
		E("%s = ", render_simple_operand(target));
	}

	E("%s(stack, sp", render_simple_operand(function));

	int i;
	for (i = 0; i < arguments; i++)
	{
		E(", %s", render_simple_operand(args[i]));
	}

	E(")\n");

	/* Release all operands. */

	if (target)
		release_operand(state, target);
	release_operand(state, function);
	for (i = 0; i < arguments; i++)
		release_operand(state, args[i]);
}

/* Generate a branch, conditional or otherwise. */

static void generate_branch(struct instruction *insn, struct bb_state *state)
{
	if (insn->cond)
	{
		struct operand* cond = get_source_operand(state, insn->cond);
		int ctype = get_base_type_of_pseudo(insn->cond);

		zprintf("if %s ~= %s then __GOTO = 0x%08x else __GOTO = 0x%08x end\n",
				render_simple_operand(cond),
				(ctype == TYPE_PTR) ? "nil" :
					"0",
				(unsigned) insn->bb_true,
				(unsigned) insn->bb_false);

		release_operand(state, cond);
	}
	else
		zprintf("__GOTO = 0x%08x\n", insn->bb_true);
}

/* Generate a phisrc instruction --- turns into a copy. */

static void generate_phisrc(struct instruction *insn, struct bb_state *state)
{
	struct operand* src = get_source_operand(state, insn->phi_src);
	if (!src)
	{
		EC("-- src is void, omitting instruction\n");
		return;
	}

	//assert(src->type == OP_REG);

	EC("-- src = %s\n", show_pseudo(insn->phi_src));

	int count = 0;
	struct instruction* phi;
	FOR_EACH_PTR(insn->phi_users, phi)
	{
		struct operand* dest = get_dest_operand(state, phi->target);

		zprintf("%s = %s\n", render_simple_operand(dest),
				render_simple_operand(src));

		release_operand(state, dest);
		count++;
	}
	END_FOR_EACH_PTR(phi);

	//assert(count == 1);
#if 0
	struct instruction* phi;
	FOR_EACH_PTR(insn->phi_users, phi)
	{
		struct operand* dest = get_dest_operand(state, phi->target);

		zprintf("%s = %s\n", render_simple_operand(dest),
				render_simple_operand(src));

		put_pseudo_in_hardreg(state, insn->phi_src, dest->reg);

		release_operand(state, dest);
	}
	END_FOR_EACH_PTR(phi);
#endif

	release_operand(state, src);
}

/* Generate code for a single instruction. */

static void generate_one_insn(struct instruction* insn, struct bb_state* state)
{
	EC("-- INSN: %s\n", show_instruction(insn));

	switch (insn->opcode)
	{
		case OP_ENTRY:
			break;

		case OP_DEATHNOTE:
			mark_pseudo_as_dead(state, insn->target);
			/* NOTE: does not break, because we don't want to call
			 * kill_dying_pseudos()! */
			return;

		case OP_LOAD:
			generate_load(insn, state);
			break;

		case OP_STORE:
			generate_store(insn, state);
			break;

		case OP_SETVAL:
			generate_setval(insn, state);
			break;

		case OP_CAST:
		case OP_SCAST:
		case OP_FPCAST:
		case OP_PTRCAST:
			generate_cast(insn, state);
			break;

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
			generate_binop(insn, state);
			break;

		case OP_SEL:
			generate_triop(insn, state);
			break;

		case OP_NEG:
		case OP_COPY:
			generate_uniop(insn, state);
			break;

		case OP_PHISOURCE:
			generate_phisrc(insn, state);
			break;

		case OP_PHI:
			/* Does nothing! */
			break;

		case OP_RET:
			generate_ret(insn, state);
			break;

		case OP_CALL:
		case OP_INLINED_CALL:
			generate_call(insn, state);
			break;

		case OP_BR:
			generate_branch(insn, state);
			break;

#if 0
		/*
		 * OP_SETVAL likewise doesn't actually generate any
		 * code. On use, the "def" of the pseudo will be
		 * looked up.
		 */
		case OP_SETVAL:
			break;

		case OP_STORE:
			generate_store(insn, state);
			break;

		case OP_LOAD:
			generate_load(insn, state);
			break;

		case OP_DEATHNOTE:
			mark_pseudo_dead(state, insn->target);
			return;

		case OP_COPY:
			generate_copy(state, insn);
			break;

	 		generate_binop(state, insn);
			break;

		case OP_BINCMP ... OP_BINCMP_END:
			generate_compare(state, insn);
			break;

		case OP_CAST: case OP_SCAST: case OP_FPCAST: case OP_PTRCAST:
			generate_cast(state, insn);
			break;

		case OP_SEL:
			generate_select(state, insn);
			break;

		case OP_SWITCH:
			generate_switch(state, insn);
			break;

		case OP_ASM:
			generate_asm(state, insn);
			break;

		case OP_PHI:
		case OP_PHISOURCE:
#endif

		default:
			printf("unimplemented: %s\n", show_instruction(insn));
			assert(0);
			E("unimplemented: %s\n", show_instruction(insn));
			break;
	}

	kill_dying_pseudos(state);
}

static void connect_storage_list(struct storage_hash_list* list)
{
	struct storage_hash *entry;
	FOR_EACH_PTR(list, entry)
	{
		pseudo_t pseudo = entry->pseudo;
		struct pinfo* pinfo = lookup_pinfo_of_pseudo(pseudo);

		if (!pinfo->reg && !pinfo->stacked)
		{
			assert(pinfo->wire);

			put_pseudo_in_hardreg(NULL, entry->pseudo, pinfo->wire);

			EC("-- import/export pseudo %s ==> hardreg %s\n",
					show_pseudo(entry->pseudo), show_hardreg(pinfo->reg));
		}
	}
	END_FOR_EACH_PTR(entry);

}

/* Actually generate the code for a bb, setting up the inputs and outputs
 * and iterating through all the instructions in the bb's code. */

static void generate_bb(struct basic_block *bb, struct bb_state *state)
{
	/* Reset all hardregs to empty; we'll explicitly import any used
	 * registers in the next stage. */

	reset_hardregs();

	/* Since the hardregs are empty, mark all pinfos as no longer being in
	 * registers. */

	reset_pinfo();

	/* Hook up the bb's inputs and outputs. This could be done a great deal
	 * more efficiently (hooking up the output here ties up a hardreg for
	 * the entire lifetime of the bb), but this ought to at least work.
	 */

	connect_storage_list(state->inputs);
	connect_storage_list(state->outputs);

	E("__LABEL = 0x%08x\n", (int) bb);

	struct instruction* insn;
	FOR_EACH_PTR(bb->insns, insn)
	{
		if (!insn->bb)
			continue;
		generate_one_insn(insn, state);
	}
	END_FOR_EACH_PTR(insn);

#if 1
	{
		struct storage_hash *entry;
		EC("--- in ---\n");
		FOR_EACH_PTR(state->inputs, entry) {
			EC("-- %s (%p) <- %s (%p)\n", show_pseudo(entry->pseudo), entry->pseudo,
					show_storage(entry->storage), entry->storage);
		} END_FOR_EACH_PTR(entry);
		EC("--- spill ---\n");
		FOR_EACH_PTR(state->internal, entry) {
			EC("-- %s <-> %s\n", show_pseudo(entry->pseudo),  show_storage(entry->storage));
		} END_FOR_EACH_PTR(entry);
		EC("--- out ---\n");
		FOR_EACH_PTR(state->outputs, entry) {
			EC("-- %s (%p) -> %s (%p)\n", show_pseudo(entry->pseudo), entry->pseudo,
					show_storage(entry->storage), entry->storage);
		} END_FOR_EACH_PTR(entry);
	}
#endif

	E("\n");
}

/* Mark all the output registers of all the parents
 * as being "used" - this does not mean that we cannot
 * re-use them, but it means that we cannot ask the
 * parents to pass in another pseudo in one of those
 * registers that it already uses for another child.
 */

static void mark_used_registers(struct basic_block *bb, struct bb_state *state)
{
	struct basic_block *parent;
	FOR_EACH_PTR(bb->parents, parent)
	{
		struct storage_hash_list *outputs = gather_storage(parent, STOR_OUT);

		struct storage_hash *entry;
		FOR_EACH_PTR(outputs, entry)
		{
			struct storage *s = entry->storage;
			if (s->type == REG_REG)
			{
				struct hardreg *reg = hardregs + s->regno;
				reg->used = 1;
			}
		}
		END_FOR_EACH_PTR(entry);
	}
	END_FOR_EACH_PTR(parent);
}

/* Generate code for a bb and all bbs that it depends on and which depend on
 * it. */

static void generate_bb_recursively(struct basic_block *bb, unsigned long generation)
{
	struct bb_state state;

	bb->generation = generation;

	/* Ensure that the parent bbs of this one get generated first. */

	generate_bb_list(bb->parents, generation);

	/* Collect information from the front end about what storages are
	 * used by this bb. */

	state.pos = bb->pos;
	state.inputs = gather_storage(bb, STOR_IN);
	state.outputs = gather_storage(bb, STOR_OUT);
	state.internal = NULL;

	/* Mark incoming registers used */
	mark_used_registers(bb, &state);

	generate_bb(bb, &state);

	free_ptr_list(&state.inputs);
	free_ptr_list(&state.outputs);

	/* Now generate all the children bbs. */

	generate_bb_list(bb->children, generation);
}

/* Generate a sequence of bbs. */

static void generate_bb_list(struct basic_block_list *list,
		unsigned long generation)
{
	struct basic_block *bb;
	FOR_EACH_PTR(list, bb)
	{
		if (bb->generation == generation)
			continue;
		generate_bb_recursively(bb, generation);
	}
	END_FOR_EACH_PTR(bb);
}

/* Set up the hardregs so that they're correctly pointing at where the
 * function's arguments are. */

static void wire_up_arguments(struct entrypoint* ep, struct basic_block* bb)
{
	/* Iterate through each argument. */

	struct instruction* entry = ep->entry;

	pseudo_t arg;
	FOR_EACH_PTR(entry->arg_list, arg)
	{
		struct pinfo* pinfo = lookup_pinfo_of_pseudo(arg);

		pinfo->stacked = 1;
		pinfo->stackoffset = stacksize++;

		printf("-- arg %s on stack at offset %d\n", show_pseudo(arg),
				pinfo->stackoffset);
	}
	END_FOR_EACH_PTR(arg);
}

/* Generate the function prologue. */

static void generate_function_body(struct entrypoint* ep)
{
	/* Write out the function header. */

	printf("%s = function(stack, fp", show_symbol_mangled(ep->name));

	struct instruction* entry = ep->entry;

	int count = 0;
	pseudo_t arg;
	FOR_EACH_PTR(entry->arg_list, arg)
	{
		printf(", H%d", arg->nr - 1);
		count++;
	}
	END_FOR_EACH_PTR(arg);

	printf(")\n");

	/* Adjust stack, and put all arguments onto it. */

	printf("local sp = fp + %d\n", stacksize);
	FOR_EACH_PTR(entry->arg_list, arg)
	{
		struct pinfo* pinfo = lookup_pinfo_of_pseudo(arg);
		assert(pinfo->stacked);

		printf("stackwrite(stack, fp, %d, H%d)\n",
				pinfo->stackoffset, arg->nr - 1);
	}
	END_FOR_EACH_PTR(arg);

	/* Declare all used registers. Registers that were used for argument
	 * passing are automatically local. */

	int i;
	for (i = count; i < NUM_REGS; i++)
	{
		struct hardreg* reg = &hardregs[i];
		if (reg->touched)
			printf("local %s\n", show_hardreg(reg));
	}

	/* Emit the actual function code. */

	zflush();

	printf("end\n\n");
}

/* Main code generation entrypoint: generate all code for the specified ep.
 * (AFAICT, an ep represents a function.)
 */

void generate_ep(struct entrypoint *ep)
{
    /* Recursively rewrite the code to decompose instructions into more
     * primitive forms. */

	unsigned long generation = ++bb_generation;
	rewrite_bb_recursively(ep->entry->bb, generation);

	/* Mark all hardregs as untouched, so we can keep track of which ones
	 * were used. */

	reset_hardregs();
	untouch_hardregs();

	/* We're using no stack space. */

	stacksize = 0;

	/* Insert deathnotes before the instruction where a register is used
	 * last. */

	track_pseudo_death(ep);

	/* Convert out of SSA form. */

	//unssa(ep);

	/* Preload the hardregs with the input arguments. */

	wire_up_arguments(ep, ep->entry->bb);

	/* Set up initial inter-bb storage links. */

	set_up_storage(ep);

	/* Wire together all the bbs. */

	generation = ++bb_generation;
	wire_up_bb_recursively(ep->entry->bb, generation);

	/* Generate the code itself into the zbuffer. */

	generation = ++bb_generation;
	generate_bb_recursively(ep->entry->bb, generation);

	/* Now generate the function body with the zbuffer contents embedded
	 * within. */

	generate_function_body(ep);

	/* Clear the storage hashes for the next function.. */
	free_storage();
}
