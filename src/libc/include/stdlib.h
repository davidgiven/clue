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

#ifndef CLUE_STDLIB_H
#define CLUE_STDLIB_H

typedef unsigned int size_t;

extern void* malloc(size_t s);
extern void* calloc(size_t s1, size_t s2);
extern void free(void* ptr);
extern void* realloc(void* ptr, size_t s);

#define atoi atol
extern long atol(const char* s);
extern long strtol(const char* nptr, char** endptr, int base);

#define NULL (void*)0

#define inline /* */

#endif
