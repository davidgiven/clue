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

enum
{
	TYPE_NONE = 0,
	TYPE_ANY,
	TYPE_INT,
	TYPE_FLOAT,
	TYPE_PTR,
	TYPE_FNPTR,
	TYPE_FN,
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
	unsigned busy : 16;
	unsigned number : 8;
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
	unsigned created : 1;              /* have we created storage for this symbol? */
	unsigned declared : 1;             /* has this symbol been declared? */
	unsigned defined : 1;              /* has this symbol been defined? */
	unsigned anonymous : 1;            /* is this symbol anonymous? */
};


extern struct hardreg hardregs[NUM_REGS];

extern const char* aprintf(const char* fmt, ...);
extern void zprintf(const char* fmt, ...);
extern void zvprintf(const char* fmt, va_list ap);
extern void zflush(void);

extern void E(const char* format, ...);
extern void EC(const char* format, ...);

extern void init_register_allocator(void);
extern const char* show_hardreg(struct hardreg* reg);
extern const char* show_hardregref(struct hardregref* hrf);
extern void reset_hardregs(void);
extern void untouch_hardregs(void);
extern struct hardreg* allocate_hardreg(void);

extern void ref_hardregref(struct hardregref* hrf);
extern void unref_hardregref(struct hardregref* hrf);
extern void find_hardregref(struct hardregref* hrf, pseudo_t pseudo);
extern void create_hardregref(struct hardregref* hrf, pseudo_t pseudo);
extern void clone_ptr_hardregref(struct hardregref* src, struct hardregref* hrf,
		pseudo_t pseudo);

extern void put_pseudo_in_hardregref(pseudo_t pseudo, struct hardregref* hrf);

extern void mark_pseudo_as_dying(pseudo_t pseudo);
extern void kill_dying_pseudos(void);
extern struct hardreg* find_source_hardreg_for_pseudo(struct bb_state* state,
		pseudo_t pseudo);
extern struct storage_hash* find_storagehash_for_pseudo(
		struct bb_state* state, pseudo_t pseudo, struct hardreg* reg);
extern void wire_up_bb_recursively(struct basic_block* bb,
    unsigned long generation);

extern void generate_ep(struct entrypoint* ep);

extern int compile_symbol_list(struct symbol_list *list);

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

#endif
