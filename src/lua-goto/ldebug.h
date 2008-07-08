/* Hacked Lua compiler; see README for more info
 *
 * © 2008 David Given.
 * Clue is licensed under the Revised BSD open source license. To get the
 * full license text, see the README file.
 *
 * $Id: build 136 2008-03-22 19:00:08Z dtrg $
 * $HeadURL: https://primemover.svn.sf.net/svnroot/primemover/pm/lib/c.pm $
 * $LastChangedDate: 2007-04-30 22:41:42 +0000 (Mon, 30 Apr 2007) $
 */

/*
** $Id: ldebug.h,v 2.3.1.1 2007/12/27 13:02:25 roberto Exp $
** Auxiliary functions from Debug Interface module
** See Copyright Notice in lua.h
*/

#ifndef ldebug_h
#define ldebug_h


#include "lstate.h"


#define pcRel(pc, p)	(cast(int, (pc) - (p)->code) - 1)

#define getline(f,pc)	(((f)->lineinfo) ? (f)->lineinfo[pc] : 0)

#define resethookcount(L)	(L->hookcount = L->basehookcount)


LUAI_FUNC void luaG_typeerror (lua_State *L, const TValue *o,
                                             const char *opname);
LUAI_FUNC void luaG_concaterror (lua_State *L, StkId p1, StkId p2);
LUAI_FUNC void luaG_aritherror (lua_State *L, const TValue *p1,
                                              const TValue *p2);
LUAI_FUNC int luaG_ordererror (lua_State *L, const TValue *p1,
                                             const TValue *p2);
LUAI_FUNC void luaG_runerror (lua_State *L, const char *fmt, ...);
LUAI_FUNC void luaG_errormsg (lua_State *L);
LUAI_FUNC int luaG_checkcode (const Proto *pt);
LUAI_FUNC int luaG_checkopenop (Instruction i);

#endif
