/* cg.c
 * Main code generator
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

static int stacksize;

/* Copy a hardreg into another hardreg. */

static void copy_hardregref(struct hardregref* src, struct hardregref* dest)
{
	cg->copy(src->simple, dest->simple);
	if (dest->type == TYPE_PTR)
		cg->copy(src->base, dest->base);
}

/* Load data into memory. */

static void generate_load(struct instruction *insn, struct bb_state *state)
{
	switch (get_base_type_of_pseudo(insn->target))
	{
		case TYPE_STRUCT:
		{
			/* Ignore structure loads: all the logic for these happens in the
			 * store instruction.
			 */
			cg->comment("structure load nop\n");
			break;
		}

		default:
		{
			struct hardregref address;
			struct hardregref dest;

			find_hardregref(&address, insn->src);
			create_hardregref(&dest, insn->target);

			assert(address.type == TYPE_PTR);

			cg->load(address.simple, address.base, insn->offset, dest.simple);
			if (dest.type == TYPE_PTR)
				cg->load(address.simple, address.base, insn->offset+1, dest.base);
		}
	}
}

/* Store data into memory. */

static void generate_store(struct instruction *insn, struct bb_state *state)
{
	switch (get_base_type_of_pseudo(insn->target))
	{
		case TYPE_STRUCT:
		{
			/* The target pseudo represents the structure. This must have come
			 * from a structure load instruction, so we follow the definition
			 * chain to find it and figure out the source address.
			 */

			assert(insn->target->def->opcode == OP_LOAD);
			assert(insn->target->def->size == insn->size);
			assert(insn->target->def->target->type == insn->target->type);
			assert(insn->target->def->target->nr == insn->target->nr);
			pseudo_t srcpseudo = insn->target->def->src;

			/* Currently no support for offsetted structure copies. */

			assert(insn->target->def->offset == 0);
			assert(insn->offset == 0);

			struct hardregref src;
			find_hardregref(&src, srcpseudo);

			struct hardregref dest;
			find_hardregref(&dest, insn->src);

			cg->comment("structure copy from %s to %s size %d\n",
					show_hardregref(&src), show_hardregref(&dest),
					bits_to_bytes(insn->size));

			cg->memcpy(&src, &dest, bits_to_bytes(insn->size));
			break;
		}

		default:
		{
			struct hardregref address;
			struct hardregref src;

			find_hardregref(&address, insn->src);
			find_hardregref(&src, insn->target);

			assert(address.type == TYPE_PTR);

			cg->store(address.simple, address.base, insn->offset, src.simple);
			if (src.type == TYPE_PTR)
				cg->store(address.simple, address.base, insn->offset+1, src.base);
		}
	}
}

/* Ensure a symbol is correctly stacked, if necessary. */

static int check_symbol_stackage(pseudo_t pseudo)
{
	assert(pseudo->type == PSEUDO_SYM);

	struct pinfo* pinfo = lookup_pinfo_of_pseudo(pseudo);

	if (!pinfo->stacked)
	{
		/* Check to see whether this symbol lives on the stack,
		 * and if so lazily allocate it a slot.
		 */

		struct symbol *sym = pseudo->sym;
		if ((sym->ctype.modifiers &
				(MOD_EXTERN | MOD_TOPLEVEL | MOD_STATIC)))
			return 0;

		/* This symbol lives on the stack. We assign them lazily. */

		int size = bits_to_bytes(sym->bit_size);
		cg->comment("allocating %d bytes on stack for %s\n", size,
				show_symbol_mangled(sym));
		pinfo->stacked = 1;
		pinfo->stackoffset = stacksize;
		stacksize += size;
	}

	return pinfo->stacked;
}

/* Load a pseudo into a register. */

static void emit_load(struct hardregref* dest, pseudo_t pseudo)
{
	switch (pseudo->type)
	{
		case PSEUDO_VOID:
			cg->comment("ignoring --- void\n");
			break;

		case PSEUDO_VAL:
			cg->set_int(pseudo->value, dest->simple);
			break;

		case PSEUDO_ARG:
		{
			struct pinfo* pinfo = lookup_pinfo_of_pseudo(pseudo);

			assert(!pinfo->stacked);
			assert(pinfo->reg.type);
			copy_hardregref(&pinfo->reg, dest);
			break;
		}

		case PSEUDO_REG:
		{
			struct hardregref src;
			create_hardregref(&src, pseudo);

			copy_hardregref(&src, dest);
			break;
		}

		case PSEUDO_SYM:
		{
			struct pinfo* pinfo = lookup_pinfo_of_pseudo(pseudo);

			if (check_symbol_stackage(pseudo))
			{
				cg->copy(&stackbase_reg, dest->base);
				cg->set_int(pinfo->stackoffset, dest->simple);
				cg->add(&frameoffset_reg, dest->simple, dest->simple);
			}
			else
			{
				/* This symbol is a global. */

				if (dest->type == TYPE_FNPTR)
					cg->set_fsymbol(pseudo->sym, dest->simple);
				else
				{
					cg->set_osymbol(pseudo->sym, dest->base);
					cg->set_int(cg->pointer_zero_offset, dest->simple);
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
				cg->set_osymbol(NULL, dest.base);
				cg->set_int(cg->pointer_zero_offset, dest.simple);
				break;

			case TYPE_FNPTR:
				assert(expr->value == 0);
				cg->set_fsymbol(NULL, dest.simple);
				break;

			default:
			{
				switch (expr->type)
				{
					case EXPR_VALUE:
						cg->set_int(expr->value, dest.simple);
						break;

					case EXPR_FVALUE:
						cg->set_float(expr->fvalue, dest.simple);
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

	struct hardregref dest;
	create_hardregref(&dest, insn->target);

	cg->comment("cast from %d to %d\n", src.type, dest.type);

	if ((src.type == TYPE_FLOAT) && (dest.type == TYPE_INT))
		cg->toint(src.simple, dest.simple);
	else if ((src.type == TYPE_INT) && (dest.type == TYPE_FLOAT))
		cg->copy(src.simple, dest.simple);
	else if (src.type == dest.type)
		copy_hardregref(&src, &dest);
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
			void (*select)(struct hardreg* cond,
					struct hardreg* dest1, struct hardreg* dest2,
					struct hardreg* true1, struct hardreg* true2,
					struct hardreg* false1, struct hardreg* false2);
			struct hardreg* cond;

			switch (src1.type)
			{
				case TYPE_PTR:
					select = cg->select_ptr;
					cond = src1.base;
					break;

				case TYPE_FNPTR:
					select = cg->select_ptr;
					cond = src1.simple;
					break;

				default:
					select = cg->select_arith;
					cond = src1.simple;
			}

			switch (dest.type)
			{
				case TYPE_PTR:
					select(cond,
							dest.simple, dest.base,
							src2.simple, src2.base,
							src3.simple, src3.base);
					break;

				default:
					select(cond,
							dest.simple, NULL,
							src2.simple, NULL,
							src3.simple, NULL);
					break;
			}
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

	struct hardregref dest;

	/* These instructions can operate on either pointers or integers, and
	 * so need to be handled specially. */
	switch (insn->opcode)
	{
		case OP_ADD:
			if (src1.type == TYPE_PTR)
			{
				clone_ptr_hardregref(&src1, &dest, insn->target);
				cg->copy(src1.base, dest.base);
			}
			else if (src2.type == TYPE_PTR)
			{
				clone_ptr_hardregref(&src2, &dest, insn->target);
				cg->copy(src2.base, dest.base);
			}
			else
				create_hardregref(&dest, insn->target);

			cg->add(src1.simple, src2.simple, dest.simple);
			return;

		case OP_SUB:
			if ((src1.type == TYPE_PTR) && (src2.type == TYPE_INT))
				clone_ptr_hardregref(&src1, &dest, insn->target);
			else
				create_hardregref(&dest, insn->target);

			cg->subtract(src1.simple, src2.simple, dest.simple);
			return;
	}

	/* Everything else operates on simple numbers only. */

	assert(src1.type != TYPE_PTR);
	assert(src2.type != TYPE_PTR);

	create_hardregref(&dest, insn->target);

	switch (insn->opcode)
	{
		case OP_MULU:
		case OP_MULS:
			cg->multiply(src1.simple, src2.simple, dest.simple);
			break;

		case OP_AND:
			cg->logand(src1.simple, src2.simple, dest.simple);
			break;

		case OP_OR:
			cg->logor(src1.simple, src2.simple, dest.simple);
			break;

		case OP_XOR:
			cg->logxor(src1.simple, src2.simple, dest.simple);
			break;

		case OP_AND_BOOL:
			cg->booland(src1.simple, src2.simple, dest.simple);
			break;

		case OP_OR_BOOL:
			cg->boolor(src1.simple, src2.simple, dest.simple);
			break;

		case OP_DIVU:
		case OP_DIVS:
			cg->divide(src1.simple, src2.simple, dest.simple);
			if ((src1.type != TYPE_FLOAT) && (src2.type != TYPE_FLOAT))
				cg->toint(dest.simple, dest.simple);
			break;

		case OP_MODU:
		case OP_MODS:
			cg->mod(src1.simple, src2.simple, dest.simple);
			break;

		case OP_SHL:
			cg->shl(src1.simple, src2.simple, dest.simple);
			break;

		case OP_LSR:
		case OP_ASR:
			cg->shr(src1.simple, src2.simple, dest.simple);
			break;

		case OP_SET_GT:
		case OP_SET_A:
			cg->set_gt(src1.simple, src2.simple, dest.simple);
			break;

		case OP_SET_LT:
		case OP_SET_B:
			cg->set_lt(src1.simple, src2.simple, dest.simple);
			break;

		case OP_SET_GE:
		case OP_SET_AE:
			cg->set_ge(src1.simple, src2.simple, dest.simple);
			break;

		case OP_SET_LE:
		case OP_SET_BE:
			cg->set_le(src1.simple, src2.simple, dest.simple);
			break;

		case OP_SET_EQ:
			cg->set_eq(src1.simple, src2.simple, dest.simple);
			break;

		case OP_SET_NE:
			cg->set_ne(src1.simple, src2.simple, dest.simple);
			break;
	}
}

/* Produce a simple 1op instruction. */

static void generate_uniop(struct instruction *insn, struct bb_state *state)
{
	switch (insn->opcode)
	{
		case OP_NEG:
		{
			struct hardregref dest;
			create_hardregref(&dest, insn->target);

			struct hardregref src;
			find_hardregref(&src, insn->src);

			cg->negate(src.simple, dest.simple);
			break;
		}

		case OP_COPY:
		{
			if (insn->src->type == PSEUDO_SYM)
			{
				if (check_symbol_stackage(insn->src))
				{
					/* This symbol is on the stack, so we can use an optimised
					 * way of calculating the pointer.
					 */
					struct pinfo* pinfo = lookup_pinfo_of_pseudo(insn->src);

					struct hardregref src;
					src.type = TYPE_PTR;
					src.simple = &frameoffset_reg;
					src.base = &stackbase_reg;

					struct hardregref dest;
					clone_ptr_hardregref(&src, &dest, insn->target);
					if (pinfo->stackoffset > 0)
					{
						cg->set_int(pinfo->stackoffset, dest.simple);
						cg->add(src.simple, dest.simple, dest.simple);
					}
					else
						cg->copy(src.simple, dest.simple);
					return;
				}
			}

			struct hardregref dest;
			create_hardregref(&dest, insn->target);

			emit_load(&dest, insn->src);
			break;
		}

		default:
			assert(0);
	}
}

/* Return from the function. */

static void generate_ret(struct instruction *insn, struct bb_state *state)
{
	if (insn->src && (insn->src->type != PSEUDO_VOID))
	{
		struct hardregref src;
		find_hardregref(&src, insn->src);

		if (src.type == TYPE_PTR)
			cg->ret(src.simple, src.base);
		else
			cg->ret(src.simple, NULL);
	}
	else
		cg->ret(NULL, NULL);
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
			cg->call(function.simple,
					target.simple, target.base);
		else
			cg->call(function.simple,
					target.simple, NULL);
	}
	else
		cg->call(function.simple,
				NULL, NULL);

	cg->call_arg(&stackoffset_reg);
	cg->call_arg(&stackbase_reg);

	struct symbol* declared = insn->func->def->symbol->sym->ctype.base_type;
	int numargs = ptr_list_size((struct ptr_list*) declared->arguments);

	pseudo_t arg;
	FOR_EACH_PTR(insn->arguments, arg)
	{
		struct hardregref hrf;
		find_hardregref(&hrf, arg);

		if (numargs > 0)
		{
			cg->call_arg(hrf.simple);
			if (hrf.type == TYPE_PTR)
				cg->call_arg(hrf.base);
			numargs--;
		}
		else
		{
			cg->call_vararg(hrf.simple);
			if (hrf.type == TYPE_PTR)
				cg->call_vararg(hrf.base);
		}
	}
	END_FOR_EACH_PTR(arg);

	cg->call_end();
}

/* Generate a branch, conditional or otherwise. */

static void generate_branch(struct instruction *insn, struct bb_state *state)
{
	struct binfo* true_binfo = lookup_binfo_of_basic_block(insn->bb_true);

	if (insn->cond)
	{
		struct hardregref hrf;
		find_hardregref(&hrf, insn->cond);

		struct binfo* false_binfo = lookup_binfo_of_basic_block(insn->bb_false);

		switch (hrf.type)
		{
			case TYPE_PTR:
				cg->bb_end_if_ptr(hrf.base, true_binfo, false_binfo);
				break;

			default:
				cg->bb_end_if_arith(hrf.simple, true_binfo, false_binfo);
				break;
		}
	}
	else
		cg->bb_end_jump(true_binfo);
}

/* Generate a phisrc instruction --- turns into a copy. */

static void generate_phisrc(struct instruction *insn, struct bb_state *state)
{
	struct instruction* phi;
	FOR_EACH_PTR(insn->phi_users, phi)
	{
		struct hardregref dest;
		create_hardregref(&dest, phi->target);

		emit_load(&dest, insn->phi_src);
	}
	END_FOR_EACH_PTR(phi);
}

/* Generate code for a single instruction. */

static void generate_one_insn(struct instruction* insn, struct bb_state* state)
{
	cg->comment("INSN: %s\n", show_instruction(insn));

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

			cg->comment("import/export pseudo %s ==> hardregref %s\n",
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

	struct binfo* binfo = lookup_binfo_of_basic_block(bb);
	cg->bb_start(binfo);

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
		cg->comment("--- in ---\n");
		FOR_EACH_PTR(state->inputs, entry) {
			cg->comment("%s (%p) <- %s (%p)\n", show_pseudo(entry->pseudo), entry->pseudo,
					show_storage(entry->storage), entry->storage);
		} END_FOR_EACH_PTR(entry);
		cg->comment("--- spill ---\n");
		FOR_EACH_PTR(state->internal, entry) {
			cg->comment("%s <-> %s\n", show_pseudo(entry->pseudo),  show_storage(entry->storage));
		} END_FOR_EACH_PTR(entry);
		cg->comment("--- out ---\n");
		FOR_EACH_PTR(state->outputs, entry) {
			cg->comment("%s (%p) -> %s (%p)\n", show_pseudo(entry->pseudo), entry->pseudo,
					show_storage(entry->storage), entry->storage);
		} END_FOR_EACH_PTR(entry);
	}
#endif
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

/* Generate code for a particular binfo. */

static void generate_binfo(struct binfo* binfo)
{
	struct bb_state state;
	struct basic_block* bb = binfo->bb;

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

		pinfo->stacked = 0;
		create_hardregref(&pinfo->wire, arg);

		cg->comment("arg %s in hardregref %s\n", show_pseudo(arg),
				show_hardregref(&pinfo->wire));

		NEXT_PTR_LIST(declaredarg);
	}
	END_FOR_EACH_PTR(arg);
	FINISH_PTR_LIST(declaredarg);
}

/* Generate the function prologue. */

static void generate_function_body(struct entrypoint* ep)
{
	/* Write out the function header. */

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

	zsetbuffer(ZBUFFER_FUNCTION);
	struct symbol* fn = ep->name->ctype.base_type;
	int returntype = get_base_type_of_symbol(fn->ctype.base_type);
	cg->function_prologue(ep->name,
			find_regclass_for_returntype(returntype));

	int i;
	cg->function_prologue_arg(&frameoffset_reg);
	cg->function_prologue_arg(&stackbase_reg);
	for (i = 0; i<count; i++)
		cg->function_prologue_arg(&hardregs[i]);
	if (fn->variadic)
		cg->function_prologue_vararg();

	/* Declare all used registers. Registers that were used for argument
	 * passing are automatically local. */

	cg->function_prologue_reg(&stackoffset_reg);
	for (i = count; i < NUM_REGS; i++)
	{
		struct hardreg* reg = &hardregs[i];
		if (reg->touched)
			cg->function_prologue_reg(reg);
	}

	cg->function_prologue_end();

	/* Adjust stack. */

	cg->set_int(stacksize, &stackoffset_reg);
	cg->add(&frameoffset_reg, &stackoffset_reg, &stackoffset_reg);

	/* Emit the actual function code. */

	zsetbuffer(ZBUFFER_FUNCTIONCODE);
	zflush(ZBUFFER_FUNCTION);

	zsetbuffer(ZBUFFER_FUNCTION);

	cg->function_epilogue();
}

/* Main code generation entrypoint: generate all code for the specified ep.
 * (AFAICT, an ep represents a function.)
 */

void generate_ep(struct entrypoint *ep)
{
	zsetbuffer(ZBUFFER_FUNCTIONCODE);

	/* Reset various bits of state. */

	reset_binfo();

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

	int sequence = 0;
	generation = ++bb_generation;
	wire_up_bb_recursively(ep->entry->bb, generation, &sequence);

	/* Generate the code itself into the zbuffer. */

	struct binfo** binfolist;
	int binfocount;
	get_binfo_list(&binfolist, &binfocount);
	int i;
	for (i = 0; i < binfocount; i++)
	{
		dump_bb(binfolist[i]->bb);
		generate_binfo(binfolist[i]);
	}

	/* Now generate the function body with the zbuffer contents embedded
	 * within. */

	generate_function_body(ep);

	/* Clear the storage hashes for the next function.. */
	free_storage();
}
