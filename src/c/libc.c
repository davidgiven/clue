/* libc.c
 * Clue C libc.
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
#include <math.h>
#include <sys/time.h>
#include <string.h>
#include <time.h>
#include <stdarg.h>
#include <assert.h>
#include "clue-crt.h"

/****************************************************************************
 *                                  STDIO                                   *
 ****************************************************************************/

clue_slot_t __stdin[1];
clue_slot_t __stdout[1];
clue_slot_t __stderr[1];

clue_int_t _printf(clue_int_t sp, clue_optr_t stack,
		clue_int_t formatpo, clue_optr_t formatpd,
		...)
{
	int cs = clue_cleanup_start();
	int chars = 0;
	va_list ap;

	va_start(ap, formatpd);
	for (;;)
	{
		int c = formatpd[formatpo].i;
		formatpo++;

		switch (c)
		{
			case '\0':
				clue_cleanup_end(cs);
				va_end(ap);
				return chars;

			case '%':
			{
				char formatbuffer[32];
				int i = 0;

				formatbuffer[0] = '%';
				i = 1;

				for (;;)
				{
					c = formatpd[formatpo].i;
					formatpo++;
					formatbuffer[i] = c;
					i++;

					if (strchr("diouxXeEfFgGaAcspn%", c))
						break;
				}

				formatbuffer[i] = '\0';

				switch (c)
				{
					case 'd':
					case 'i':
					case 'o':
					case 'u':
					case 'x':
					case 'X':
					{
						clue_int_t i = va_arg(ap, clue_int_t);
						chars += printf(formatbuffer, i);
						break;
					}

					case 'e':
					case 'E':
					case 'f':
					case 'F':
					case 'g':
					case 'G':
					case 'a':
					case 'A':
					{
						clue_real_t r = va_arg(ap, clue_real_t);
						chars += printf(formatbuffer, r);
						break;
					}

					case 'c':
					{
						clue_int_t c = va_arg(ap, clue_int_t);
						putchar(c);
						chars++;
						break;
					}

					case '%':
					{
						putchar('%');
						chars++;
						break;
					}

					case 's':
					{
						clue_int_t po = va_arg(ap, clue_int_t);
						clue_optr_t pd = va_arg(ap, clue_optr_t);
						const char* s = clue_ptrtostring(po, pd);

						chars += printf("%s", s);

						free((void*) s);
						break;
					}

					case 'p':
					{
						clue_int_t po = va_arg(ap, clue_int_t);
						clue_optr_t pd = va_arg(ap, clue_optr_t);

						chars += printf("[PTR:%08X+%08X]", pd, po);
						break;
					}

					case 'n':
					{
						clue_int_t po = va_arg(ap, clue_int_t);
						clue_optr_t pd = va_arg(ap, clue_optr_t);

						pd[po].i = chars;
						break;
					}
				}
				break;
			}

			default:
				putchar(c);
				chars++;
		}
	}
}

clue_int_t _putc(clue_int_t sp, clue_optr_t stack,
		clue_int_t c,
		clue_int_t po, clue_optr_t pd)
{
	if (pd == __stdout)
		return putc(c, stdout);
	else if (pd == __stderr)
		return putc(c, stderr);
	return 0;
}

clue_int_t _atol(clue_int_t sp, clue_optr_t stack,
		clue_int_t po, clue_optr_t pd)
{
	const char* s = clue_ptrtostring(po, pd);
	clue_int_t result = atol(s);
	free((void*) s);
	return result;
}

/****************************************************************************
 *                                 STRINGS                                  *
 ****************************************************************************/

clue_ptr_pair_t _strcpy(clue_int_t sp, clue_optr_t stack,
		clue_int_t destpo, clue_optr_t destpd,
		clue_int_t srcpo, clue_optr_t srcpd)
{
	clue_ptr_pair_t _r;
	_r.i = destpo;
	_r.o = destpd;

	for (;;)
	{
		clue_int_t c = srcpd[srcpo].i;
		destpd[destpo].i = c;

		srcpo++;
		destpo++;

		if (!c)
			break;
	}

	return _r;
}

clue_ptr_pair_t _memset(clue_int_t sp, clue_optr_t stack,
		clue_int_t destpo, clue_optr_t destpd,
		clue_int_t c, clue_int_t n)
{
	for (int i = 0; i < (n-1); i++)
		destpd[destpo + i].i = c;

	clue_ptr_pair_t r;
	r.i = destpo;
	r.o = destpd;
	return r;
}


/****************************************************************************
 *                                  TIME                                    *
 ****************************************************************************/

clue_int_t _gettimeofday(clue_int_t sp, clue_optr_t stack,
		clue_int_t tvpo, clue_optr_t tvpd,
		clue_int_t tzpo, clue_optr_t tzpd)
{
	struct timeval tv;
	gettimeofday(&tv, NULL);

	tvpd[tvpo+0].i = tv.tv_sec;
	tvpd[tvpo+1].i = tv.tv_usec;
	return 0;
}

/****************************************************************************
 *                                  MATHS                                   *
 ****************************************************************************/

#define MATHSWRAPPER(NAME) \
	clue_real_t _##NAME (clue_int_t sp, clue_optr_t stack, clue_real_t n) \
	{ return NAME(n); }

MATHSWRAPPER(sin)
MATHSWRAPPER(cos)
MATHSWRAPPER(atan)
MATHSWRAPPER(log)
MATHSWRAPPER(exp)
MATHSWRAPPER(sqrt)

clue_real_t _pow(clue_int_t sp, clue_optr_t stack, clue_real_t x, clue_real_t y)
{
	return pow(x, y);
}

/****************************************************************************
 *                                 MEMORY                                   *
 ****************************************************************************/

clue_ptr_pair_t _malloc(clue_int_t sp, clue_optr_t stack, clue_int_t size)
{
	clue_ptr_pair_t _r;

	_r.i = 0;
	_r.o = calloc(sizeof(clue_slot_t), size);

	return _r;
}

clue_ptr_pair_t _calloc(clue_int_t sp, clue_optr_t stack,
		clue_int_t size1, clue_int_t size2)
{
	clue_ptr_pair_t _r;

	_r.i = 0;
	_r.o = calloc(sizeof(clue_slot_t), size1*size2);

	return _r;
}

void _free(clue_int_t sp, clue_optr_t stack,
		clue_int_t po, clue_optr_t pd)
{
	assert(po == 0);
	free(pd);
}

