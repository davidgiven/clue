/* Clue libc headers
 *
 * © 2008 David Given.
 * Clue is licensed under the Revised BSD open source license. To get the
 * full license text, see the README file.
 *
 * $Id: build 136 2008-03-22 19:00:08Z dtrg $
 * $HeadURL: https://primemover.svn.sf.net/svnroot/primemover/pm/lib/c.pm $
 * $LastChangedDate: 2007-04-30 22:41:42 +0000 (Mon, 30 Apr 2007) $
 */

#ifndef CLUE_STDIO_H
#define CLUE_STDIO_H

#include <stdlib.h>

extern int printf(const char* format, ...);

typedef struct FILE FILE;
extern FILE _stdin;
extern FILE _stdout;
extern FILE _stderr;
#define stdin (&_stdin)
#define stdout (&_stdout)
#define stderr (&_stderr)

extern int putc(int c, FILE* fp);

#endif
