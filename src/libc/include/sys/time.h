/* Clue libc headers
 *
 * © 2008 David Given.
 * Clue is licensed under the Revised BSD open source license. To get the
 * full license text, see the README file.
 *
 * $Id$
 * $HeadURL$
 * $LastChangedDate: 2007-04-30 22:41:42 +0000 (Mon, 30 Apr 2007) $
 */

#ifndef CLUE_SYS_TIME_H
#define CLUE_SYS_TIME_H

typedef signed long time_t;
typedef unsigned long suseconds_t;

struct timeval
{
	time_t tv_sec;
	suseconds_t tv_usec;
};

struct timezone;

extern int gettimeofday(struct timeval* tv, struct timezone* tz);

#endif
