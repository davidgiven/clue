/* globals.h
 * Global settings
 *
 * © 2008 David Given.
 * Clue is licensed under the Revised BSD open source license. To get the
 * full license text, see the README file.
 *
 * $Id: build 136 2008-03-22 19:00:08Z dtrg $
 * $HeadURL: https://primemover.svn.sf.net/svnroot/primemover/pm/lib/c.pm $
 * $LastChangedDate: 2007-04-30 22:41:42 +0000 (Mon, 30 Apr 2007) $
 */

#ifndef GLOBALS_H
#define GLOBALS_H

#define _GNU_SOURCE
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <fcntl.h>
#include <assert.h>

#include "sparse/lib.h"
#include "sparse/allocate.h"
#include "sparse/token.h"
#include "sparse/parse.h"
#include "sparse/symbol.h"
#include "sparse/expression.h"
#include "sparse/linearize.h"
#include "sparse/flow.h"
#include "sparse/storage.h"
#include "sparse/target.h"

#define NUM_REGS 200
#define MAX_ARGS 32

/* Available output buffers. */

enum
{
	ZBUFFER_STDOUT = -1,
	ZBUFFER_CODE = 0,
	ZBUFFER_FUNCTION,
	ZBUFFER_FUNCTIONCODE,
	ZBUFFER_HEADER,
	ZBUFFER_INITIALIZER,
	ZBUFFER__MAX
};

enum
{
	TYPE_NONE = 0,
	TYPE_ANY,
	TYPE_VOID,
	TYPE_INT,
	TYPE_FLOAT,
	TYPE_PTR,
	TYPE_FNPTR,
	TYPE_FN,
	TYPE_STRUCT,
};

/* Register types. */

enum
{
	REGTYPE_INT    = 1<<0,
	REGTYPE_FLOAT  = 1<<1,
	REGTYPE_BOOL   = 1<<2,
	REGTYPE_OPTR   = 1<<3,
	REGTYPE_FPTR   = 1<<4,

	REGTYPE_ALL    = (1<<5)-1,
	NUM_REG_CLASSES = 5
};

/* Special register classes. */

enum
{
	REGCLASS_VOID = -1,
	REGCLASS_REGPAIR = -2
};

struct bb_state
{
	struct position pos;
	struct storage_hash_list *inputs;
	struct storage_hash_list *outputs;
	struct storage_hash_list *internal;
};

struct pinfo;
DECLARE_PTR_LIST(pinfo_list, struct pinfo);

struct hardreg
{
	const char* name;
	unsigned busy : 16;
	unsigned number : 8;
	unsigned regclass : 8;
	unsigned used : 1;
	unsigned touched : 1;
	unsigned dying : 1;
};

/* Represents a reference to a hardreg or register pair. */

struct hardregref
{
	int type;
	struct hardreg* simple;
	struct hardreg* base;    /* Valid if type == TYPE_PTR */
};

/* pinfos store the backend-specific data about a pseudo: notably,
 * what the C type is (int, float etc), and what register it's currently
 * stored in. Note that just to make life more exciting, a given hardreg
 * may store several pseudos at once (in the phi case).
 */

struct pinfo
{
	pseudo_t pseudo;
	int type;
	int size;
	struct hardregref reg;
	struct hardregref wire;
	unsigned int stackoffset;
	unsigned dying : 1;
	unsigned stacked : 1;
};

/* sinfos store back-end specific data about pseudos,
 */

struct sinfo
{
	struct symbol* sym;
	const char* name;                  /* mangled name */
	unsigned here : 1;                 /* is this symbol defined in this file? */
	unsigned declared : 1;             /* has this symbol been declared? */
	unsigned defined : 1;              /* has this symbol been defined? */
	unsigned anonymous : 1;            /* is this symbol anonymous? */
};

/* binfos store back-end specific data about basic blocks.
 */

struct binfo
{
	struct basic_block* bb;
	int id;                            /* sequence number for this bb */
};

extern struct hardreg stackbase_reg;
extern struct hardreg stackoffset_reg;
extern struct hardreg frameoffset_reg;
extern struct hardreg hardregs[NUM_REGS];

/* Represents a code generator backend. */

struct codegenerator
{
	int pointer_zero_offset;
	const char* spname;
	const char* fpname;
	const char* stackname;
	int register_class[NUM_REG_CLASSES];

	void (*reset_registers)(void);
	void (*init_register)(struct hardreg* reg, int regclass);
	const char* (*get_register_name)(struct hardreg* reg);

	void (*prologue)(void);
	void (*epilogue)(void);
	void (*comment)(const char* format, ...);

	void (*declare_function)(struct symbol* sym, int returning);
	void (*declare_function_arg)(int regclass);
	void (*declare_function_vararg)(void);
	void (*declare_function_end)(void);

	void (*declare_slot)(struct symbol* sym, unsigned size);
	void (*create_storage)(struct symbol* sym, unsigned size);
	void (*import)(struct symbol* sym);
	void (*export)(struct symbol* sym);

	/* These functions will always be called in order (_arg and _reg may
	 * be called multiple times).
	 */
	void (*function_prologue)(struct symbol* name, int returning);
	void (*function_prologue_arg)(struct hardreg* reg);
	void (*function_prologue_vararg)(void);
	void (*function_prologue_reg)(struct hardreg* reg);
	void (*function_prologue_end)(void);

	void (*function_epilogue)(void);

	void (*bb_start)(struct binfo* bb);
	void (*bb_end_jump)(struct binfo* target);
	void (*bb_end_if_arith)(struct hardreg* cond,
			struct binfo* truetarget, struct binfo* falsetarget);
	void (*bb_end_if_ptr)(struct hardreg* cond,
			struct binfo* truetarget, struct binfo* falsetarget);

	void (*copy)(struct hardreg* src, struct hardreg* dest);
	void (*load)(struct hardreg* simple, struct hardreg* base, int offset,
			struct hardreg* dest);
	void (*store)(struct hardreg* simple, struct hardreg* base, int offset,
			struct hardreg* src);

	void (*set_int)(long long int value, struct hardreg* dest);
	void (*set_float)(long double value, struct hardreg* dest);
	void (*set_osymbol)(struct symbol* sym, struct hardreg* dest);
	void (*set_fsymbol)(struct symbol* sym, struct hardreg* dest);

	void (*toint)(struct hardreg* src, struct hardreg* dest);
	void (*negate)(struct hardreg* src, struct hardreg* dest);
	void (*add)(struct hardreg* src1, struct hardreg* src2, struct hardreg* dest);
	void (*subtract)(struct hardreg* src1, struct hardreg* src2, struct hardreg* dest);
	void (*multiply)(struct hardreg* src1, struct hardreg* src2, struct hardreg* dest);
	void (*divide)(struct hardreg* src1, struct hardreg* src2, struct hardreg* dest);
	void (*mod)(struct hardreg* src1, struct hardreg* src2, struct hardreg* dest);
	void (*shl)(struct hardreg* src1, struct hardreg* src2, struct hardreg* dest);
	void (*shr)(struct hardreg* src1, struct hardreg* src2, struct hardreg* dest);
	void (*logand)(struct hardreg* src1, struct hardreg* src2, struct hardreg* dest);
	void (*logor)(struct hardreg* src1, struct hardreg* src2, struct hardreg* dest);
	void (*logxor)(struct hardreg* src1, struct hardreg* src2, struct hardreg* dest);
	void (*booland)(struct hardreg* src1, struct hardreg* src2, struct hardreg* dest);
	void (*boolor)(struct hardreg* src1, struct hardreg* src2, struct hardreg* dest);
	void (*set_gt)(struct hardreg* src1, struct hardreg* src2, struct hardreg* dest);
	void (*set_ge)(struct hardreg* src1, struct hardreg* src2, struct hardreg* dest);
	void (*set_lt)(struct hardreg* src1, struct hardreg* src2, struct hardreg* dest);
	void (*set_le)(struct hardreg* src1, struct hardreg* src2, struct hardreg* dest);
	void (*set_eq)(struct hardreg* src1, struct hardreg* src2, struct hardreg* dest);
	void (*set_ne)(struct hardreg* src1, struct hardreg* src2, struct hardreg* dest);

	void (*select_arith)(struct hardreg* cond,
			struct hardreg* dest1, struct hardreg* dest2,
			struct hardreg* true1, struct hardreg* true2,
			struct hardreg* false1, struct hardreg* false2);
	void (*select_ptr)(struct hardreg* cond,
			struct hardreg* dest1, struct hardreg* dest2,
			struct hardreg* true1, struct hardreg* true2,
			struct hardreg* false1, struct hardreg* false2);

	/* These are always called in order (okay, one call_returning(), followed
	 * by zero or more call_reg(), followed by call_end()).
	 */
	void (*call)(struct hardreg* func,
			struct hardreg* dest1, struct hardreg* dest2);
	void (*call_arg)(struct hardreg* arg);
	void (*call_vararg)(struct hardreg* arg);
	void (*call_end)(void);

	void (*ret)(struct hardreg* simple, struct hardreg* base);

	void (*memcpy)(struct hardregref* src, struct hardregref* dest, int size);
};

extern const struct codegenerator* cg;
extern const struct codegenerator cg_lua;
extern const struct codegenerator cg_javascript;
extern const struct codegenerator cg_perl5;
extern const struct codegenerator cg_c;

extern const char* aprintf(const char* fmt, ...);
extern void zprintf(const char* fmt, ...);
extern void zvprintf(const char* fmt, va_list ap);
extern void zflush(int output);
extern void zsetbuffer(int buffer);

extern void init_register_allocator(void);
extern const char* show_hardreg(struct hardreg* reg);
extern const char* show_hardregref(struct hardregref* hrf);
extern void reset_hardregs(void);
extern void untouch_hardregs(void);

extern void ref_hardregref(struct hardregref* hrf);
extern void unref_hardregref(struct hardregref* hrf);
extern void find_hardregref(struct hardregref* hrf, pseudo_t pseudo);
extern void create_hardregref(struct hardregref* hrf, pseudo_t pseudo);
extern void clone_ptr_hardregref(struct hardregref* src, struct hardregref* hrf,
		pseudo_t pseudo);
extern int find_regclass_for_returntype(int type);
extern int find_regclass_for_regtype(int regtype);
extern struct hardreg* allocate_hardreg(int regtype);
extern void unref_hardreg(struct hardreg* reg);

extern int is_pseudo_in_register(pseudo_t pseudo);

extern void put_pseudo_in_hardregref(pseudo_t pseudo, struct hardregref* hrf);

extern void mark_pseudo_as_dying(pseudo_t pseudo);
extern void kill_dying_pseudos(void);
extern struct hardreg* find_source_hardreg_for_pseudo(struct bb_state* state,
		pseudo_t pseudo);
extern struct storage_hash* find_storagehash_for_pseudo(
		struct bb_state* state, pseudo_t pseudo, struct hardreg* reg);
extern void wire_up_bb_recursively(struct basic_block* bb,
    unsigned long generation, int* sequence);

extern void generate_ep(struct entrypoint* ep);

extern int compile_symbol_list(struct symbol_list *list);
extern void emit_initializer(void);

extern const char* show_simple_pseudo(struct bb_state* state, pseudo_t pseudo);
extern const char* show_value(struct expression* expr);

extern void reset_pinfo(void);
extern struct pinfo* lookup_pinfo_of_pseudo(pseudo_t pseudo);

extern int lookup_base_type_of_pseudo(pseudo_t pseudo);
extern int get_base_type_of_pseudo(pseudo_t pseudo);
extern int get_base_type_of_symbol(struct symbol* symbol);

extern struct sinfo* lookup_sinfo_of_symbol(struct symbol* sym);
extern const char* show_symbol_mangled(struct symbol* sym);

extern void rewrite_bb_recursively(struct basic_block* bb,
    unsigned long generation);

struct binfo* lookup_binfo_of_basic_block(struct basic_block* binfo);

extern void dump_fn(struct entrypoint *ep);

#endif
