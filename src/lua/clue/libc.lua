-- clue.crt
--
-- Clue C libc
--
-- © 2008 David Given.
-- Clue is licensed under the Revised BSD open source license. To get the
-- full license text, see the README file.
--
-- $Id$

require "clue.crt"
require "socket"

local unpack = unpack
local type = type
local print = print

local ptrread = clue.crt.ptrread
local ptrwrite = clue.crt.ptrwrite
local ptroffset = clue.crt.ptroffset
local ptrtostring = clue.crt.ptrtostring
local newptr = clue.crt.newptr
local math_floor = math.floor
local tonumber = tonumber
local io_write = io.write
local io_stdin = io.stdin
local io_stdout = io.stdout
local io_stderr = io.stderr
local string_format = string.format
local string_gsub = string.gsub
local math_sin = math.sin
local math_cos = math.cos
local math_sqrt = math.sqrt
local math_pow = math.pow
local math_log = math.log
local math_atan = math.atan
local math_exp = math.exp
local socket_gettime = socket.gettime

module "clue.libc"

function _malloc(sp, stack, size)
	return 1, {}
end

function _calloc(sp, stack, size1, size2)
	local d = {}
	for i = 1, (size1*size2) do
		d[i] = 0
	end
	return 1, d
end
	
function _free(sp, stack, po, pd)
end

function _realloc(sp, stack, po, pd)
	return po, pd
end

function _memcpy(sp, stack, destpo, destpd, srcpo, srcpd, count)
	for offset = 0, count-1 do
		destpd[destpo+offset] = srcpd[srcpo+offset]
	end
	return destpo, destpd
end

function _memset(sp, stack, destpo, destpd, c, n)
	for offset = 0, n-1 do
		destpd[destpo+offset] = c
	end
	return destpo, destpd
end
	
function _atoi(sp, stack, po, pd)
	local s = ptrtostring(po, pd)
	return math_floor(tonumber(s))
end

_atol = _atoi

function _printf(sp, stack, formatpo, formatpd, ...)
	format = ptrtostring(formatpo, formatpd)
	local inargs = {...}
	local outargs = {}
	
	local inargcount = #inargs
	local i = 1
	while (i <= inargcount) do
		local thisarg = inargs[i]
		i = i + 1
		if (i <= inargcount) then
			local nextarg = inargs[i]
			if (nextarg == nil) or (type(nextarg) == "table") then
				-- If the next argument is nil or a table, then we must
				-- be looking at a register pair representing a pointer.
				thisarg = ptrtostring(thisarg, nextarg)
				i = i + 1
			end
		end
		
		outargs[#outargs+1] = thisarg
	end	

	-- Massage the format string to be Lua-compatible.
	
	format = string_gsub(format, "[^%%]%%l", "%%")
	format = string_gsub(format, "^%%l", "%%")
	
	-- Use Lua's string.format to actually do the rendering.
	
	io_write(string_format(format, unpack(outargs)))
	return 1
end

__stdin = io_stdin
__stdout = io_stdout
__stderr = io_stderr
 
function _putc(sp, stack, c, fppo, fppd)
	fppd:write(string_format("%c", c))
end

-----------------------------------------------------------------------------
--                                STRINGS                                  --
-----------------------------------------------------------------------------

function _strcpy(sp, stack, destpo, destpd, srcpo, srcpd)
	local origdestpo = destpo
	
	while true do
		local c = srcpd[srcpo]
		destpd[destpo] = c
		
		srcpo = srcpo + 1
		destpo = destpo + 1
		
		if (c == 0) then
			break
		end
	end
	
	return origdestpo, destpd
end

-----------------------------------------------------------------------------
--                                 MATHS                                   --
-----------------------------------------------------------------------------

function _sin(sp, stack, x)
	return math_sin(x)
end

function _cos(sp, stack, x)
	return math_cos(x)
end

function _sqrt(sp, stack, x)
	return math_sqrt(x)
end

function _pow(sp, stack, x, y)
	return math_pow(x, y)
end

function _log(sp, stack, x)
	return math_log(x)
end

function _atan(sp, stack, x)
	return math_atan(x)
end

function _exp(sp, stack, x)
	return math_exp(x)
end

-----------------------------------------------------------------------------
--                                 TIME                                    --
-----------------------------------------------------------------------------

function _gettimeofday(sp, stack, tvpo, tvpd, tzpo, tzpd)
	local t = socket_gettime()
	local secs = math_floor(t)
	local usecs = math_floor((t - secs) * 1000000)
	tvpd[tvpo+0] = secs
	tvpd[tvpo+1] = usecs
	return 0
end
