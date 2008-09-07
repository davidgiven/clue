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

#ifndef CLUE_STRING_H
#define CLUE_STRING_H

extern void* memcpy(const void* src, void* dest, size_t s);
extern void* memset(void* s, int c, size_t n);
extern char* strcpy(char* dest, const char* src);

#endif
