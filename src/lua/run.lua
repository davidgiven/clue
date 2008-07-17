#!/usr/bin/lua
-- Clue runtime loader
--
-- © 2008 David Given.
-- Clue is licensed under the Revised BSD open source license. To get the
-- full license text, see the README file.
--
-- $Id: cluerun 20 2008-07-16 10:21:58Z dtrg $
-- $HeadURL: https://cluecc.svn.sourceforge.net/svnroot/cluecc/clue/cluerun $
-- $LastChangedDate: 2008-07-16 11:21:58 +0100 (Wed, 16 Jul 2008) $

-- You may need to alter these two lines if LuaJit expects to see modules
-- somewhere other than the default location. (For example, if you use the
-- Debian packages for Lua, but install LuaJit from source.)

package.path = package.path ..
	";/usr/share/lua/5.1/?.lua;/usr/share/lua/5.1/?/init.lua;" ..
	"/usr/lib/lua/5.1/?.lua;/usr/lib/lua/5.1/?/init.lua"
	
package.cpath = package.cpath ..
	";/usr/lib/lua/5.1/?.so;/usr/lib/lua/5.1/?.so;/usr/lib/lua/5.1/loadall.so"

-- This line allows us to find the clue modules. Don't change.

package.path = package.path ..
	";src/lua/?.lua"

require "socket"
require "clue.crt"
require "clue.libc"

-- Import the standard library.

for k, v in pairs(clue.libc) do
	_G[k] = v
end

-- Load the binaries.

local argc = 3
local argv = {...}
while true do
	local f = argv[argc]
	if (f == "--") then
		argc = argc + 1
		break
	end
	if (f == nil) then
		print("Command line does not contain a --")
		os.exit(1)
	end
		
	local chunk, e = loadfile(argv[argc])
	if e then
		print("Load error: "..e)
		os.exit(1)
	end

	chunk()
	argc = argc + 1
end

-- Run it.

do
	-- (Construct argv list first.)
	
	local cargs = {}

	for i = argc, #argv do	
		local v = clue.crt.newstring(argv[i])
		cargs[#cargs+1] = 1
		cargs[#cargs+1] = v
	end
	
	clue.crt.run_initializers()
	local result = _main(1, {}, #argv - argc, 1, cargs)
	os.exit(result)
end

