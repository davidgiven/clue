-- clue.crt
--
-- Clue C libc
--
-- © 2008 David Given.
-- Clue is licensed under the Revised BSD open source license. To get the
-- full license text, see the README file.
--
-- $Id: build 136 2008-03-22 19:00:08Z dtrg $

require "clue.crt"
require "socket"

local unpack = unpack
local type = type

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
local math_sin = math.sin
local math_cos = math.cos
local math_sqrt = math.sqrt
local math_pow = math.pow
local math_log = math.log
local math_atan = math.atan
local math_exp = math.exp
local socket_gettime = socket.gettime

module "clue.libc"

function _malloc(stack, sp, size)
	return newptr()
end

function _calloc(stack, sp, size1, size2)
	local d = {}
	for i = 1, (size1*size2) do
		d[i] = 0
	end
	return newptr(d)
end
	
function _free(stack, sp, p)
end

function _realloc(stack, sp, p, size)
	return p
end

function _memcpy(stack, sp, dest, src, count)
	for offset = 0, count-1 do
		local i = ptrread(src, offset)
		ptrwrite(dest, offset, i)
	end
	return dest
end

function _memset(stack, sp, ptr, c, n)
	for offset = 0, n-1 do
		ptrwrite(ptr, offset, c)
	end
	return ptr
end
	
function _atoi(stack, sp, s)
	s = ptrtostring(s)
	return math_floor(tonumber(s))
end

_atol = _atoi

function _printf(stack, sp, format, ...)
	format = ptrtostring(format)
	args = {...}
	
	for i = 1, #args do
		local v = args[i]
		if (type(v) == "table") then
			args[i] = ptrtostring(v)
		end
	end

	io_write(string_format(format, unpack(args)))
	return 1
end

_stdin = newptr({io_stdin})
_stdout = newptr({io_stdout})
_stderr = newptr({io_stderr})
 
function _putc(stack, sp, c, fp)
	fp:write(string_format("%c", c))
end

-----------------------------------------------------------------------------
--                                STRINGS                                  --
-----------------------------------------------------------------------------

function _strcpy(stack, sp, dest, src)
	local o = 0
	while true do
		local c = ptrread(src, o)
		ptrwrite(dest, o, c)
		o = o + 1
		if (c == 0) then
			break
		end
	end
	
	return dest
end

-----------------------------------------------------------------------------
--                                 MATHS                                   --
-----------------------------------------------------------------------------

function _sin(stack, sp, x)
	return math_sin(x)
end

function _cos(stack, sp, x)
	return math_cos(x)
end

function _sqrt(stack, sp, x)
	return math_sqrt(x)
end

function _pow(stack, sp, x, y)
	return math_pow(x, y)
end

function _log(stack, sp, x)
	return math_log(x)
end

function _atan(stack, sp, x)
	return math_atan(x)
end

function _exp(stack, sp, x)
	return math_exp(x)
end

-----------------------------------------------------------------------------
--                                 TIME                                    --
-----------------------------------------------------------------------------

function _gettimeofday(stack, sp, tv, tz)
	local t = socket_gettime()
	local secs = math_floor(t)
	local usecs = math_floor((t - secs) * 1000)
	ptrwrite(tv, 0, secs)
	ptrwrite(tv, 1, usecs)
	return 0
end
