/* crt.c
 * Clue C runtime.
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
#include <stdio.h>
#include <string.h>
#include "clue-crt.h"

static clue_slot_t clue_stack[1024];

static void* cleanup_stack[128];
static int cleanup_sp = 0;

int clue_cleanup_start(void)
{
	return cleanup_sp;
}

void clue_cleanup_push(void* ptr)
{
	cleanup_sp++;
	cleanup_stack[cleanup_sp] = ptr;
}

void clue_cleanup_end(int cs)
{
	while (cleanup_sp > cs)
	{
		free(cleanup_stack[cleanup_sp]);
		cleanup_sp--;
	}
}

const char* clue_ptrtostring(clue_int_t po, clue_optr_t pd)
{
	int len = 0;

	/* Count string length */

	for (;;)
	{
		int c = pd[po+len].i;
		len++;
		if (c == '\0')
			break;
	}

	/* Allocate string. */

	char* s = malloc(len);

	/* Copy string data */

	int i;
	for (i = 0; i < len; i++)
		s[i] = pd[po+i].i;

	return s;
}

clue_optr_t clue_makestring(const char* s)
{
	int len = strlen(s);
	clue_optr_t clues = calloc(sizeof(clue_slot_t), len+1);

	for (int i = 0; i < len; i++)
		clues[i].i = s[i];

	return clues;
}

int main(int argc, const char* argv[])
{
	clue_slot_t clueargv[(argc+1)*2];

	for (int i = 0; i < argc; i++)
	{
		clueargv[(i*2)+0].i = 0;
		clueargv[(i*2)+1].o = clue_makestring(argv[i]);
	}

	clueargv[(argc*2)+0].i = 0;
	clueargv[(argc*2)+1].o = NULL;

	clue_initializer();

	_main(0, clue_stack, argc, 0, clueargv);
}
