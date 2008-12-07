/* clue-crt.h
 * Clue C runtime interface.
 *
 * © 2008 David Given.
 * Clue is licensed under the Revised BSD open source license. To get the
 * full license text, see the README file.
 *
 * $Id$
 * $HeadURL$
 * $LastChangedDate: 2007-04-30 22:41:42 +0000 (Mon, 30 Apr 2007) $
 */

#include <stdlib.h>
#include <stdint.h>

#define CLUE_CONSTRUCTOR __attribute__ ((constructor))

#define CLUE_EMULATE_INT_WITH_DOUBLE

typedef int64_t clue_realint_t;
typedef double clue_realreal_t;

#if defined CLUE_EMULATE_INT_WITH_DOUBLE
typedef clue_realreal_t clue_int_t;
#else
typedef clue_realint_t clue_int_t;
#endif

typedef clue_realreal_t clue_real_t;
typedef clue_realint_t clue_bool_t;
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

