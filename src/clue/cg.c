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

/* Copy a hardreg. */

static void cg_copy_hardreg(struct hardreg* src, struct hardreg* dest)
{
	if (src != dest)
		E("%s = %s\n", show_hardreg(dest), show_hardreg(src));
}

/* Unpack a packed pointer into a register pair. */

static void cg_unpack_pointer(struct hardregref* hrf)
{
	E("%s = %s[1]\n",
			show_hardreg(hrf->base),
			show_hardreg(hrf->simple));
	E("%s = %s[2]\n",
			show_hardreg(hrf->simple),
			show_hardreg(hrf->simple));
}

/* Load data into memory. */

static void generate_load(struct instruction *insn, struct bb_state *state)
{
	struct hardregref address;
	struct hardregref dest;

	find_hardregref(&address, insn->src);
	create_hardregref(&dest, insn->target);

	assert(address.type == TYPE_PTR);

	E("%s = %s[%s + %d]\n",
			show_hardreg(dest.simple),
			show_hardreg(address.base),
			show_hardreg(address.simple),
			insn->offset);

	if (dest.type == TYPE_PTR)
		E("%s = %s[%s + %d]\n",
				show_hardreg(dest.base),
				show_hardreg(address.base),
				show_hardreg(address.simple),
				insn->offset + 1);
}

/* Store data into memory. */

static void generate_store(struct instruction *insn, struct bb_state *state)
{
	struct hardregref address;
	struct hardregref src;

	find_hardregref(&address, insn->src);
	find_hardregref(&src, insn->target);

	assert(address.type == TYPE_PTR);

	E("%s[%s + %d] = %s\n",
			show_hardreg(address.base),
			show_hardreg(address.simple),
			insn->offset,
			show_hardreg(src.simple));

	if (src.type == TYPE_PTR)
		E("%s[%s + %d] = %s\n",
				show_hardreg(address.base),
				show_hardreg(address.simple),
				insn->offset + 1,
				show_hardreg(src.base));
}

/* Load a pseudo into a register. */

static void emit_load(struct hardregref* dest, pseudo_t pseudo)
{
	switch (pseudo->type)
	{
		case PSEUDO_VOID:
			EC("-- ignoring --- void\n");
			break;

		case PSEUDO_VAL:
			E("%s = %lld\n", show_hardreg(dest->simple),
					pseudo->value);
			break;

		case PSEUDO_ARG:
		{
			struct pinfo* pinfo = lookup_pinfo_of_pseudo(pseudo);

			if (pinfo->stacked)
			{
				E("%s = stack[fp + %d]\n",
						show_hardreg(dest->simple),
						pinfo->stackoffset);

				if (dest->type == TYPE_PTR)
					E("%s = stack[fp + %d]\n",
							show_hardreg(dest->base),
							pinfo->stackoffset + 1);
			}
			else
			{
				assert(pinfo->reg.type);
				cg_copy_hardreg(pinfo->reg.simple, dest->simple);
				if (dest->type == TYPE_PTR)
					cg_copy_hardreg(pinfo->reg.base, dest->base);
			}
			break;
		}

		case PSEUDO_REG:
		{
			struct hardregref src;
			create_hardregref(&src, pseudo);

			E("%s = %s\n",
					show_hardreg(dest->simple), show_hardreg(src.simple));
			if (src.type == TYPE_PTR)
				E("%s = %s\n",
						show_hardreg(dest->base), show_hardreg(src.base));
			break;
		}
		case PSEUDO_SYM:
		{
			struct pinfo* pinfo = lookup_pinfo_of_pseudo(pseudo);

			if (pinfo->stacked)
			{
				/* This symbol is on the stack. */

			stacked:
				E("%s = stack\n", show_hardreg(dest->base));
				E("%s = fp + %d\n", show_hardreg(dest->simple),
						pinfo->stackoffset);
			}
			else
			{
				/* Check to see whether this symbol lives on the stack,
				 * and if so lazily allocate it a slot.
				 */
				struct symbol *sym = pseudo->sym;
				if (!(sym->ctype.modifiers &
						(MOD_EXTERN | MOD_TOPLEVEL | MOD_STATIC)))
				{
					/* This symbol lives on the stack. We assign them lazily. */

					int size = bits_to_bytes(sym->bit_size);
					EC("-- allocating %d bytes on stack for %s\n", size,
							show_symbol_mangled(sym));
					pinfo->stacked = 1;
					pinfo->stackoffset = stacksize;
					stacksize += size;
					goto stacked;
				}

				/* This symbol is a global. */

				if (dest->type == TYPE_FNPTR)
				{
					E("%s = %s\n",
							show_hardreg(dest->simple),
							show_symbol_mangled(sym));
				}
				else
				{
					E("%s = %s\n",
							show_hardreg(dest->base),
							show_symbol_mangled(sym));
					E("%s = 1\n",
							show_hardreg(dest->simple));
				}
			}
			break;
		}

		default:
			assert(0);
	}
}

static void generate_setval(struct instruction *insn, struct bb_state *state)
{
	struct hardregref dest;
	create_hardregref(&dest, insn->target);

	pseudo_t pseudo = insn->symbol;

	if (pseudo)
		emit_load(&dest, pseudo);
	else
	{
		struct expression* expr = insn->val;

		switch (dest.type)
		{
			case TYPE_PTR:
				assert(expr->value == 0);
				E("%s = nil\n", show_hardreg(dest.simple));
				E("%s = nil\n", show_hardreg(dest.base));
				break;

			default:
			{
				const char* dests = show_hardreg(dest.simple);

				switch (expr->type)
				{
					case EXPR_VALUE:
						E("%s = %lld\n", dests, expr->value);
						break;

					case EXPR_FVALUE:
						E("%s = %llf\n", dests, expr->fvalue);
						break;

					default:
						printf("(expression type %d)\n", expr->type);
						assert(0);
				}
			}
		}
	}
}

/* Cast one type to another. */

static void generate_cast(struct instruction *insn, struct bb_state *state)
{
	struct hardregref src;
	find_hardregref(&src, insn->src);
	const char* srcs = show_hardreg(src.simple);

	struct hardregref dest;
	create_hardregref(&dest, insn->target);
	const char* dests = show_hardreg(dest.simple);

	EC("-- cast from %d to %d\n", src.type, dest.type);

	if ((src.type == TYPE_FLOAT) && (dest.type == TYPE_INT))
		E("%s = int(%s)\n", dests, srcs);
	else if ((src.type == TYPE_INT) && (dest.type == TYPE_FLOAT))
		E("%s = %s\n", dests, srcs);
	else if (src.type == dest.type)
	{
		E("%s = %s\n", dests, srcs);
		if (src.type == TYPE_PTR)
			E("%s = %s\n", show_hardreg(dest.base), show_hardreg(src.base));
	}
	else
	{
		sparse_error(insn->src->def->pos,
			     "this cast is not supported by clue");
	}
}

/* Produce a simple 3op instruction. */

static void generate_triop(struct instruction *insn, struct bb_state *state)
{
	struct hardregref src1;
	find_hardregref(&src1, insn->src1);

	struct hardregref src2;
	find_hardregref(&src2, insn->src2);

	struct hardregref src3;
	find_hardregref(&src3, insn->src3);

	struct hardregref dest;
	create_hardregref(&dest, insn->target);

	switch (insn->opcode)
	{
		case OP_SEL:
		{
			switch (src1.type)
			{
				case TYPE_PTR:
					E("if %s then ", show_hardreg(src1.base));
					break;

				case TYPE_FNPTR:
					E("if %s then ", show_hardreg(src1.simple));
					break;

				default:
					E("if %s ~= 0 then ", show_hardreg(src1.simple));
			}

			switch (dest.type)
			{
				case TYPE_PTR:
					E("%s = %s %s = %s else %s = %s %s = %s",
							show_hardreg(dest.simple), show_hardreg(src2.simple),
							show_hardreg(dest.base), show_hardreg(src2.base),
							show_hardreg(dest.simple), show_hardreg(src3.simple),
							show_hardreg(dest.base), show_hardreg(src3.base));
					break;

				default:
					E("%s = %s else %s = %s",
							show_hardreg(dest.simple), show_hardreg(src2.simple),
							show_hardreg(dest.simple), show_hardreg(src3.simple));
					break;
			}

			E(" end\n");
			break;
		}

		default:
			assert(0);
	}
}

/* Produce a simple 2op instruction. */

static void generate_binop(struct instruction *insn, struct bb_state *state)
{
	struct hardregref src1;
	struct hardregref src2;

	find_hardregref(&src1, insn->src1);
	find_hardregref(&src2, insn->src2);

	const char* src1s = show_hardreg(src1.simple);
	const char* src2s = show_hardreg(src2.simple);

	struct hardregref dest;

	/* These instructions can operate on either pointers or integers, and
	 * so need to be handled specially. */
	switch (insn->opcode)
	{
		case OP_ADD:
			if (src1.type == TYPE_PTR)
			{
				clone_ptr_hardregref(&src1, &dest, insn->target);
				cg_copy_hardreg(src1.base, dest.base);
			}
			else if (src2.type == TYPE_PTR)
			{
				clone_ptr_hardregref(&src2, &dest, insn->target);
				cg_copy_hardreg(src2.base, dest.base);
			}
			else
				create_hardregref(&dest, insn->target);

			E("%s = %s + %s\n", show_hardreg(dest.simple), src1s, src2s);
			return;

		case OP_SUB:
			if ((src1.type == TYPE_PTR) && (src2.type == TYPE_INT))
				clone_ptr_hardregref(&src1, &dest, insn->target);
			else
				create_hardregref(&dest, insn->target);

			E("%s = %s - %s\n", show_hardreg(dest.simple), src1s, src2s);
			return;
	}

	/* Everything else operates on simple numbers only. */

	assert(src1.type != TYPE_PTR);
	assert(src2.type != TYPE_PTR);

	create_hardregref(&dest, insn->target);
	const char* dests = show_hardreg(dest.simple);

	switch (insn->opcode)
	{
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

		case OP_DIVU:
		case OP_DIVS:
			if ((src1.type == TYPE_FLOAT) || (src2.type == TYPE_FLOAT))
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
}

/* Produce a simple 1op instruction. */

static void generate_uniop(struct instruction *insn, struct bb_state *state)
{
	struct hardregref src;
	struct hardregref dest;

	find_hardregref(&src, insn->src);
	create_hardregref(&dest, insn->target);

	const char* srcs = show_hardreg(src.simple);
	const char* dests = show_hardreg(dest.simple);

	switch (insn->opcode)
	{
		case OP_NEG:
			E("%s = -%s\n", dests, srcs);
			break;

#if 0
		case OP_PHISOURCE:
		case OP_COPY:
			E("%s = %s\n", dests, srcs);
			if (src.type == TYPE_PTR)
				E("%s = %s\n", show_hardreg(src.base),
						show_hardreg(dest.base));
			break;
#endif
		default:
			assert(0);
	}
}

/* Return from the function. */

static void generate_ret(struct instruction *insn, struct bb_state *state)
{
	E("do return");
	if (insn->src && (insn->src->type != PSEUDO_VOID))
	{
		struct hardregref src;

		find_hardregref(&src, insn->src);

		if (src.type == TYPE_PTR)
			E(" %s, %s",
					show_hardreg(src.simple),
					show_hardreg(src.base));
		else
			E(" %s", show_hardreg(src.simple));
	}
	E(" end\n");
}

/* Call a function. */

static void generate_call(struct instruction *insn, struct bb_state *state)
{
	struct hardregref function;
	find_hardregref(&function, insn->func);

	/* Emit the instruction. */

	struct hardregref target;
	target.type = TYPE_NONE;
	if (insn->target && (insn->target != VOID))
	{
		create_hardregref(&target, insn->target);
		if (target.type == TYPE_PTR)
			E("%s, %s = ", show_hardreg(target.simple), show_hardreg(target.base));
		else
			E("%s = ", show_hardreg(target.simple));
	}

	E("%s(sp, stack", show_hardreg(function.simple));

	pseudo_t arg;
	FOR_EACH_PTR(insn->arguments, arg)
	{
		struct hardregref hrf;
		find_hardregref(&hrf, arg);

		if (hrf.type == TYPE_PTR)
			E(", %s, %s", show_hardreg(hrf.simple),
					show_hardreg(hrf.base));
		else
			E(", %s", show_hardreg(hrf.simple));
	}
	END_FOR_EACH_PTR(arg);

	E(")\n");
}

/* Generate a branch, conditional or otherwise. */

static void generate_branch(struct instruction *insn, struct bb_state *state)
{
	if (insn->cond)
	{
		struct hardregref hrf;

		find_hardregref(&hrf, insn->cond);
		switch (hrf.type)
		{
			case TYPE_PTR:
				E("if %s then __GOTO = 0x%08x else __GOTO = 0x%08x end\n",
						show_hardreg(hrf.base), insn->bb_true, insn->bb_false);
				break;

			default:
				E("if %s ~= 0 then __GOTO = 0x%08x else __GOTO = 0x%08x end\n",
						show_hardreg(hrf.simple), insn->bb_true, insn->bb_false);
				break;
		}
	}
	else
		zprintf("__GOTO = 0x%08x\n", insn->bb_true);
}

/* Generate a phisrc instruction --- turns into a copy. */

static void generate_phisrc(struct instruction *insn, struct bb_state *state)
{
#if 0
	struct operand* src = get_source_operand(state, insn->phi_src);
	if (!src)
	{
		EC("-- src is void, omitting instruction\n");
		return;
	}
#endif

//	int count = 0;
	struct instruction* phi;
	FOR_EACH_PTR(insn->phi_users, phi)
	{
		struct hardregref dest;
		create_hardregref(&dest, phi->target);

		emit_load(&dest, insn->phi_src);

//		count++;
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
			if (insn->target->type == PSEUDO_REG)
				mark_pseudo_as_dying(insn->target);
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

	kill_dying_pseudos();
}

static void connect_storage_list(struct storage_hash_list* list)
{
	struct storage_hash *entry;
	FOR_EACH_PTR(list, entry)
	{
		pseudo_t pseudo = entry->pseudo;
		struct pinfo* pinfo = lookup_pinfo_of_pseudo(pseudo);

		if (!pinfo->reg.type && !pinfo->stacked)
		{
			assert(pinfo->wire.type);

			ref_hardregref(&pinfo->wire);
			put_pseudo_in_hardregref(entry->pseudo, &pinfo->wire);

			EC("-- import/export pseudo %s ==> hardregref %s\n",
					show_pseudo(entry->pseudo), show_hardregref(&pinfo->reg));
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
	/* Iterate through each argument and each declared argument
	 * simultaneously. */

	struct instruction* entry = ep->entry;
	struct symbol* declared = ep->name->ctype.base_type;

	pseudo_t arg;
	struct symbol* declaredarg;
	PREPARE_PTR_LIST(declared->arguments, declaredarg);
	FOR_EACH_PTR(entry->arg_list, arg)
	{
		struct pinfo* pinfo = lookup_pinfo_of_pseudo(arg);
		int size = bits_to_bytes(pinfo->size);

		if (declaredarg && !(declaredarg->ctype.modifiers & MOD_ADDRESSABLE))
		{
			/* Put this argument in a register. */

			pinfo->stacked = 0;
			create_hardregref(&pinfo->wire, arg);

			printf("-- arg %s in hardregref %s\n", show_pseudo(arg),
					show_hardregref(&pinfo->wire));
		}
		else
		{
			/* Otherwise stack it. */

			pinfo->stacked = 1;
			pinfo->stackoffset = stacksize;
			stacksize += size;

			printf("-- arg %s on stack at offset %d, size %d\n", show_pseudo(arg),
					pinfo->stackoffset, size);
		}

		NEXT_PTR_LIST(declaredarg);
	}
	END_FOR_EACH_PTR(arg);
	FINISH_PTR_LIST(declaredarg);
}

/* Generate the function prologue. */

static void generate_function_body(struct entrypoint* ep)
{
	/* Write out the function header. */

	printf("%s = function(fp, stack", show_symbol_mangled(ep->name));

	struct instruction* entry = ep->entry;

	int count = 0;
	pseudo_t arg;
	FOR_EACH_PTR(entry->arg_list, arg)
	{
		struct pinfo* pinfo = lookup_pinfo_of_pseudo(arg);
		int size = bits_to_bytes(pinfo->size);
		count += size;
	}
	END_FOR_EACH_PTR(arg);

	int i;
	for (i = 0; i<count; i++)
		printf(", H%d", i);

	printf(")\n");

	/* Adjust stack, and put all arguments onto it. */

	printf("local sp = fp + %d\n", stacksize);
	i = 0;
	FOR_EACH_PTR(entry->arg_list, arg)
	{
		struct pinfo* pinfo = lookup_pinfo_of_pseudo(arg);
		int size = bits_to_bytes(pinfo->size);

		if (pinfo->stacked)
		{
			int j;
			for (j = 0; j < size; j++)
				printf("stack[fp + %d] = H%d\n",
						pinfo->stackoffset+j, i+j);
		}

		i += size;
	}
	END_FOR_EACH_PTR(arg);

	/* Declare all used registers. Registers that were used for argument
	 * passing are automatically local. */

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
	/* Mark all hardregs as untouched, so we can keep track of which ones
	 * were used. */

	reset_hardregs();
	untouch_hardregs();

	/* Preload the hardregs with the input arguments. */

	wire_up_arguments(ep, ep->entry->bb);

    /* Recursively rewrite the code to decompose instructions into more
     * primitive forms. */

	unsigned long generation = ++bb_generation;
	rewrite_bb_recursively(ep->entry->bb, generation);

	/* We're using no stack space. */

	stacksize = 0;

	/* Insert deathnotes before the instruction where a register is used
	 * last. */

	track_pseudo_death(ep);

	/* Convert out of SSA form. */

	//unssa(ep);

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
