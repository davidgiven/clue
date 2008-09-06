/* clue-crt.h
 * Clue C runtime interface.
 *
 * © 2008 David Given.
 * Clue is licensed under the Revised BSD open source license. To get the
 * full license text, see the README file.
 *
 * $Id: build 136 2008-03-22 19:00:08Z dtrg $
 * $HeadURL: https://primemover.svn.sf.net/svnroot/primemover/pm/lib/c.pm $
 * $LastChangedDate: 2007-04-30 22:41:42 +0000 (Mon, 30 Apr 2007) $
 */

#include <stdlib.h>
#include <stdint.h>

typedef int64_t clue_int_t;
typedef double clue_real_t;
typedef int clue_bool_t;
typedef union clue_slot* clue_optr_t;
typedef void (*clue_fptr_t)();

typedef union clue_slot
{
	clue_int_t i;
	clue_real_t f;
	clue_bool_t b;
	clue_optr_t o;
	clue_fptr_t p;
} clue_slot_t;

typedef struct
{
	clue_int_t i;
	clue_optr_t o;
} clue_ptr_pair_t;

extern void clue_initializer(void);

extern int clue_cleanup_start(void);
extern void clue_cleanup_push(void* ptr);
extern void clue_cleanup_end(int cs);

extern const char* clue_ptrtostring(clue_int_t po, clue_optr_t pd);

extern clue_int_t _main(clue_int_t sp, clue_optr_t stack,
		clue_int_t argc,
		clue_int_t argvpo, clue_optr_t argvpd);

extern clue_slot_t __stdin[1];
extern clue_slot_t __stdout[1];
extern clue_slot_t __stderr[1];

